#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <jni.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sstream>
#include <sys/stat.h>
#include <endian.h>

#include "excludes/excludes.h"
#include "log.h"
#include "symbol/symbol.h"

#include "matrix_hprof_analyzer.h"

using namespace matrix::hprof;

#define TAG "Matrix.MemoryUtil"

static bool task_process = false;

static std::string extract_string(JNIEnv *env, jstring string) {
    const char *value = env->GetStringUTFChars(string, nullptr);
    std::string result(value, strlen(value));
    env->ReleaseStringUTFChars(string, value);
    return result;
}

// These values need to be updated synchronously with constants in object TaskState. ***************
#define TS_UNKNOWN (-1)
#define TS_DUMP 1
#define TS_ANALYZER_CREATE 2
#define TS_ANALYZER_INITIALIZE 3
#define TS_ANALYZER_EXECUTE 4
#define TS_CREATE_RESULT_FILE 5
#define TS_SERIALIZE 5
// *************************************************************************************************

static std::string task_state_dir;

static void update_task_state(int8_t state) {
    if (!task_process) return;
    _info_log(TAG, "Update task %d state to %d.", getpid(), state);
    std::stringstream task_path;
    task_path << task_state_dir << "/" << getpid();
    int task_fd = open(task_path.str().c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (task_fd == -1) {
        _error_log(TAG, "Failed to open task state file while updating (errno: %d).", errno);
        return;
    }
    if (write(task_fd, &state, sizeof(int8_t)) != sizeof(int8_t)) {
        _error_log(TAG, "Failed to write task state into file while updating (errno: %d).", errno);
    }
    if (close(task_fd) == -1) {
        _error_log(TAG, "Failed to close task state info file while updating (errno: %d).", errno);
    }
}

static int8_t get_task_state_and_cleanup(int pid) {
    std::stringstream task_path;
    task_path << task_state_dir << "/" << pid;
    int task_fd = open(task_path.str().c_str(), O_RDONLY);
    int8_t result;
    if (task_fd == -1) {
        _error_log(TAG, "Failed to open task state file while reading (errno: %d).", errno);
        result = TS_UNKNOWN;
        goto cleanup;
    }
    if (read(task_fd, &result, sizeof(int8_t)) != sizeof(int8_t)) {
        _error_log(TAG, "Failed to write task state into file while reading (errno: %d).",
                   errno);
        result = TS_UNKNOWN;
    }
    if (close(task_fd)) {
        _error_log(TAG, "Failed to close task state info file while reading (errno: %d).",
                   errno);
    }
    cleanup:
    if (remove(task_path.str().c_str())) {
        _error_log(TAG, "Failed to delete task state info file (errno: %d).", errno);
    }
    return result;
}

static jclass task_result_class = nullptr;
static jmethodID task_result_constructor = nullptr;

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_loadJniCache(JNIEnv *env, jobject) {
    _info_log(TAG, "Load JNI pointer cache.");
    if (task_result_constructor == nullptr) {
        if (task_result_class == nullptr) {
            jclass local = env->FindClass("com/tencent/matrix/resource/MemoryUtil$TaskResult");
            if (local == nullptr) {
                _error_log(TAG, "Failed to find class TaskResult.");
                return false;
            }
            // Make sure the class will not be unloaded.
            // See: https://developer.android.com/training/articles/perf-jni#jclass-jmethodid-and-jfieldid
            task_result_class = reinterpret_cast<jclass>(env->NewGlobalRef(local));
            if (task_result_class == nullptr) {
                _error_log(TAG, "Failed to create global reference of class TaskResult.");
                return false;
            }
        }

        task_result_constructor = env->GetMethodID(task_result_class, "<init>", "(IIB)V");
        if (task_result_constructor == nullptr) {
            _error_log(TAG, "Failed to find constructor of class TaskResult.");
            return false;
        }
    }
    return true;
}

static jobject
create_task_result(JNIEnv *env, int32_t type, int32_t code, int8_t task_state) {
    return env->NewObject(task_result_class, task_result_constructor, type, code, task_state);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_syncTaskStateDir(JNIEnv *env, jobject, jstring path) {
    const char *value = env->GetStringUTFChars(path, nullptr);
    task_state_dir = std::string(value, strlen(value));
    env->ReleaseStringUTFChars(path, value);
    return !task_state_dir.empty();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_initializeSymbol(JNIEnv *, jobject) {
    _info_log(TAG, "Initialize native symbols.");
    return initialize_symbols();
}

#define unwrap_optional(optional, nullopt_action)   \
    ({                                              \
        const auto &result = optional;              \
        if (!result.has_value()) {                  \
            nullopt_action;                         \
        }                                           \
        result.value();                             \
    })

static int32_t serialize_reference_type(LeakChain::Node::ReferenceType type) {
    switch (type) {
        case LeakChain::Node::ReferenceType::kStaticField:
            return 1;
        case LeakChain::Node::ReferenceType::kInstanceField:
            return 2;
        case LeakChain::Node::ReferenceType::kArrayElement:
            return 3;
    }
}

static int32_t serialize_object_type(LeakChain::Node::ObjectType type) {
    switch (type) {
        case LeakChain::Node::ObjectType::kClass:
            return 1;
        case LeakChain::Node::ObjectType::kObjectArray:
            return 2;
        default:
            return 3;
    }
}

static int fork_task(const char *task_name, unsigned int timeout) {
    auto *thread = current_thread();
    suspend_runtime(thread);
    int pid = fork();
    if (pid == 0) {
        task_process = true;
        if (timeout != 0) {
            alarm(timeout);
        }
        prctl(PR_SET_NAME, task_name);
    } else {
        resume_runtime(thread);
    }
    return pid;
}

static void execute_dump(const char *file_name) {
    _info_log(TAG, "Start dumping in task %d.", getpid());
    update_task_state(TS_DUMP);
    dump_heap(file_name);
}

static std::optional<std::vector<LeakChain>>
execute_analyze(const char *hprof_path, const char *reference_key) {
    _info_log(TAG, "Start analyzing in task %d.", getpid());

    update_task_state(TS_ANALYZER_CREATE);
    const int hprof_fd = open(hprof_path, O_RDONLY);
    if (hprof_fd == -1) {
        _error_log(TAG, "Failed to open HPROF file.");
        return std::nullopt;
    }
    HprofAnalyzer analyzer(hprof_fd);

    update_task_state(TS_ANALYZER_INITIALIZE);
    if (!exclude_default_references(analyzer)) {
        _error_log(TAG, "Failed to add exclude default references rules.");
        return std::nullopt;
    }

    update_task_state(TS_ANALYZER_EXECUTE);
    auto result = analyzer.Analyze([reference_key](const HprofHeap &heap) {
        const object_id_t leak_ref_class_id = unwrap_optional(
                heap.FindClassByName(
                        "com.tencent.matrix.resource.analyzer.model.DestroyedActivityInfo"),
                return std::vector<object_id_t>());
        std::vector<object_id_t> leaks;
        for (const object_id_t leak_ref: heap.GetInstances(leak_ref_class_id)) {
            const object_id_t key_string_id = unwrap_optional(
                    heap.GetFieldReference(leak_ref, "mKey"), continue);
            const std::string &key_string = unwrap_optional(
                    heap.GetValueFromStringInstance(key_string_id), continue);
            if (key_string != reference_key)
                continue;
            const object_id_t weak_ref = unwrap_optional(
                    heap.GetFieldReference(leak_ref, "mActivityRef"), continue);
            const object_id_t leak = unwrap_optional(
                    heap.GetFieldReference(weak_ref, "referent"), continue);
            leaks.emplace_back(leak);
        }
        return leaks;
    });
    if (result.has_value()) {
        return result.value();
    } else {
        _error_log(TAG, HprofAnalyzer::CheckError());
        return std::nullopt;
    }
}

static bool execute_serialize(const char *result_path, const std::vector<LeakChain> &leak_chains) {
    _info_log(TAG, "Start serialize analyze result in task %d.", getpid());

    update_task_state(TS_CREATE_RESULT_FILE);
    bool result = false;
    int result_fd = open(result_path, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR);
    if (result_fd == -1) {
        _error_log(TAG, "Failed to write leak chains to result path (errno: %d).", errno);
        return false;
    }

    update_task_state(TS_SERIALIZE);
    // See comment documentation of <code>MemoryUtil.deserialize</code> for the file format of
    // result file.
#define write_data(content, size)                                                               \
        if (write(result_fd, content, size) == -1) {                                            \
            _error_log(TAG, "Failed to write content to result path (errno: %d).", errno);  \
            goto write_leak_chain_done;                                                         \
        }

    const uint32_t byte_order_magic = 0x1;
    const uint32_t leak_chain_count = leak_chains.size();
    write_data(&byte_order_magic, sizeof(uint32_t))
    write_data(&leak_chain_count, sizeof(uint32_t))
    for (const auto &leak_chain : leak_chains) {
        const uint32_t leak_chain_length = leak_chain.GetDepth() + 1;
        write_data(&leak_chain_length, sizeof(uint32_t))

        int32_t gc_root_type;
        if (leak_chain_length == 1) {
            gc_root_type = serialize_object_type(LeakChain::Node::ObjectType::kInstance);
        } else {
            const auto ref_type = leak_chain.GetNodes()[0].GetReferenceType();
            switch (ref_type) {
                case LeakChain::Node::ReferenceType::kStaticField:
                    gc_root_type = serialize_object_type(LeakChain::Node::ObjectType::kClass);
                    break;
                case LeakChain::Node::ReferenceType::kInstanceField:
                    gc_root_type = serialize_object_type(
                            LeakChain::Node::ObjectType::kInstance);
                    break;
                case LeakChain::Node::ReferenceType::kArrayElement:
                    gc_root_type = serialize_object_type(
                            LeakChain::Node::ObjectType::kObjectArray);
                    break;
            }
        }
        write_data(&gc_root_type, sizeof(int32_t))
        const uint32_t gc_root_name_length = leak_chain.GetGcRoot().GetName().length();
        write_data(&gc_root_name_length, sizeof(uint32_t))
        write_data(leak_chain.GetGcRoot().GetName().c_str(), gc_root_name_length)

        for (const auto &node : leak_chain.GetNodes()) {
            const int32_t serialized_reference_type =
                    serialize_reference_type(node.GetReferenceType());
            write_data(&serialized_reference_type, sizeof(int32_t))
            const uint32_t reference_name_length = node.GetReference().length();
            write_data(&reference_name_length, sizeof(uint32_t))
            write_data(node.GetReference().c_str(), reference_name_length)

            const int32_t serialized_object_type =
                    serialize_object_type(node.GetObjectType());
            write_data(&serialized_object_type, sizeof(int32_t))
            const uint32_t object_name_length = node.GetObject().length();
            write_data(&object_name_length, sizeof(uint32_t))
            write_data(node.GetObject().c_str(), object_name_length)
        }

        const uint32_t end_tag = 0;
        write_data(&end_tag, sizeof(uint32_t))
    }
    result = true;
    write_leak_chain_done:
    close(result_fd);
    return result;
}

#define TC_NO_ERROR 0
#define TC_EXCEPTION 1
#define TC_ANALYZE_ERROR 2
#define TC_SERIALIZE_ERROR 3

extern "C"
JNIEXPORT jint JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_forkDump(JNIEnv *env, jobject,
                                                     jstring java_hprof_path,
                                                     jlong timeout) {
    const std::string hprof_path = extract_string(env, java_hprof_path);

    int task_pid = fork_task("matrix_mem_dump", timeout);
    if (task_pid != 0) {
        return task_pid;
    } else {
        /* dump */
        execute_dump(hprof_path.c_str());
        /* end */
        _exit(TC_NO_ERROR);
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_forkAnalyze(JNIEnv *env, jobject,
                                                        jstring java_hprof_path,
                                                        jstring java_result_path,
                                                        jstring java_reference_key,
                                                        jlong timeout) {
    const std::string hprof_path = extract_string(env, java_hprof_path);
    const std::string result_path = extract_string(env, java_result_path);
    const std::string reference_key = extract_string(env, java_reference_key);

    int task_pid = fork_task("matrix_mem_anlz", timeout);
    if (task_pid != 0) {
        return task_pid;
    } else {
        /* analyze */
        const std::optional<std::vector<LeakChain>> result =
                execute_analyze(hprof_path.c_str(), reference_key.c_str());
        if (!result.has_value()) _exit(TC_ANALYZE_ERROR);
        /* serialize result */
        const bool serialized = execute_serialize(result_path.c_str(), result.value());
        if (!serialized) _exit(TC_SERIALIZE_ERROR);
        /* end */
        _exit(TC_NO_ERROR);
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_forkDumpAndAnalyze(JNIEnv *env, jobject,
                                                               jstring java_hprof_path,
                                                               jstring java_result_path,
                                                               jstring java_reference_key,
                                                               jlong timeout) {
    const std::string hprof_path = extract_string(env, java_hprof_path);
    const std::string result_path = extract_string(env, java_result_path);
    const std::string reference_key = extract_string(env, java_reference_key);

    int task_pid = fork_task("matrix_mem_d&a", timeout);
    if (task_pid != 0) {
        return task_pid;
    } else {
        /* dump */
        execute_dump(hprof_path.c_str());
        /* analyze */
        const std::optional<std::vector<LeakChain>> result =
                execute_analyze(hprof_path.c_str(), reference_key.c_str());
        if (!result.has_value()) _exit(TC_ANALYZE_ERROR);
        /* serialize result */
        const bool serialized = execute_serialize(result_path.c_str(), result.value());
        if (!serialized) _exit(TC_SERIALIZE_ERROR);
        /* end */
        _exit(TC_NO_ERROR);
    }
}

// These values need to be updated synchronously with constants in class TaskResult. ***************
#define TR_TYPE_WAIT_FAILED (-1)
#define TR_TYPE_EXIT 0
#define TR_TYPE_SIGNALED 1
#define TR_TYPE_UNKNOWN 2
// *************************************************************************************************

extern "C" JNIEXPORT jobject JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_waitTask(JNIEnv *env, jobject, jint pid) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        _error_log(TAG, "Failed to invoke waitpid().");
        return create_task_result(env, TR_TYPE_WAIT_FAILED, errno, TS_UNKNOWN);
    }

    const int8_t task_state = get_task_state_and_cleanup(pid);
    if (WIFEXITED(status)) {
        return create_task_result(env, TR_TYPE_EXIT, WEXITSTATUS(status), task_state);
    } else if (WIFSIGNALED(status)) {
        return create_task_result(env, TR_TYPE_SIGNALED, WTERMSIG(status), task_state);
    } else {
        return create_task_result(env, TR_TYPE_UNKNOWN, 0, task_state);
    }
}