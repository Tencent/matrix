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
#include <fstream>

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

static void log_and_throw_runtime_exception(JNIEnv *env, const char *message) {
    _error_log(TAG, "exception: %s", message);
    jclass runtime_exception_class = env->FindClass("java/lang/RuntimeException");
    if (runtime_exception_class != nullptr) {
        env->ThrowNew(runtime_exception_class, message);
    }
}

// These values need to be updated synchronously with constants in object TaskState. ***************
#define TS_UNKNOWN (-1)
#define TS_DUMP 1
#define TS_ANALYZER_CREATE 2
#define TS_ANALYZER_INITIALIZE 3
#define TS_ANALYZER_EXECUTE 4
#define TS_CREATE_RESULT_FILE 5
#define TS_SERIALIZE 6
// *************************************************************************************************

static std::string task_state_dir;

static void update_task_state(int8_t state) {
    if (!task_process) return;
    _info_log(TAG, "update_state: task %d state -> %d.", getpid(), state);
    std::stringstream task_path;
    task_path << task_state_dir << "/" << getpid();
    int task_fd = open(task_path.str().c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (task_fd == -1) {
        _error_log(TAG, "update_state: invoke open() failed with errno %d", errno);
        return;
    }
    if (write(task_fd, &state, sizeof(int8_t)) != sizeof(int8_t)) {
        _error_log(TAG, "update_state: invoke write() failed with errno %d", errno);
    }
    if (close(task_fd) == -1) {
        _error_log(TAG, "update_state: invoke close() failed with errno %d", errno);
    }
}

static int8_t get_task_state_and_cleanup(int pid) {
    std::stringstream task_path;
    task_path << task_state_dir << "/" << pid;
    int task_fd = open(task_path.str().c_str(), O_RDONLY);
    int8_t result;
    if (task_fd == -1) {
        _error_log(TAG, "get_state: invoke open() failed with errno %d", errno);
        result = TS_UNKNOWN;
        goto cleanup;
    }
    if (read(task_fd, &result, sizeof(int8_t)) != sizeof(int8_t)) {
        _error_log(TAG, "get_state: invoke read() failed with errno %d", errno);
        result = TS_UNKNOWN;
    }
    if (close(task_fd)) {
        _error_log(TAG, "get_state: invoke close() failed with errno %d", errno);
    }
    cleanup:
    if (access(task_path.str().c_str(), F_OK) == 0) {
        if (remove(task_path.str().c_str())) {
            _error_log(TAG, "get_state: invoke remove() failed with errno %d", errno);
        }
    }
    return result;
}

static std::string task_error_dir;

static void update_task_error(const std::string &error) {
    if (!task_process) return;
    _info_log(TAG, "update_error: task %d error -> %s", getpid(), error.c_str());
    std::stringstream task_path;
    task_path << task_error_dir << "/" << getpid();
    int task_fd = open(task_path.str().c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (task_fd == -1) {
        _error_log(TAG, "update_error: invoke open() failed with errno %d", errno);
        return;
    }
    if (write(task_fd, error.c_str(), error.length()) != error.length()) {
        _error_log(TAG, "update_error: invoke write() failed with errno %d", errno);
    }
    if (close(task_fd) == -1) {
        _error_log(TAG, "update_error: invoke close() failed with errno %d", errno);
    }
}

static std::string get_task_error_and_cleanup(int pid) {
    std::stringstream task_path;
    task_path << task_error_dir << "/" << pid;
    std::string result;
    {
        std::ifstream file(task_path.str());
        getline(file, result);
        if (file.fail()) result = "";
    }
    if (access(task_path.str().c_str(), F_OK) == 0) {
        if (remove(task_path.str().c_str())) {
            _error_log(TAG, "get_task: invoke remove() failed with errno %d", errno);
        }
    }
    return result;
}

static jclass task_result_class = nullptr;
static jmethodID task_result_constructor = nullptr;

extern "C"
JNIEXPORT void JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_loadJniCache(JNIEnv *env, jobject) {
    _info_log(TAG, "initialize: load JNI pointer cache");
    if (task_result_constructor == nullptr) {
        if (task_result_class == nullptr) {
            jclass local = env->FindClass("com/tencent/matrix/resource/MemoryUtil$TaskResult");
            if (local == nullptr) {
                log_and_throw_runtime_exception(env, "Failed to find class TaskResult");
                return;
            }
            // Make sure the class will not be unloaded.
            // See: https://developer.android.com/training/articles/perf-jni#jclass-jmethodid-and-jfieldid
            task_result_class = reinterpret_cast<jclass>(env->NewGlobalRef(local));
            if (task_result_class == nullptr) {
                log_and_throw_runtime_exception(env, "Failed to create global reference of class TaskResult");
                return;
            }
        }

        task_result_constructor =
                env->GetMethodID(task_result_class, "<init>", "(IIBLjava/lang/String;)V");
        if (task_result_constructor == nullptr) {
            log_and_throw_runtime_exception(env, "Failed to find constructor of class TaskResult");
            return;
        }
    }
}

static jobject
create_task_result(JNIEnv *env, int32_t type, int32_t code,
                   int8_t task_state, const std::string &task_error) {
    return env->NewObject(task_result_class, task_result_constructor, type, code,
                          task_state, env->NewStringUTF(task_error.c_str()));
}

// ! JNI method
static void create_directory(JNIEnv *env, const char *path) {
    if (mkdir(path, S_IRWXU) == 0) return;
    if (errno != EEXIST) {
        std::stringstream error_builder;
        error_builder << "Failed to create directory " << path
                      << " with errno " << errno;
        log_and_throw_runtime_exception(env, error_builder.str().c_str());
        return;
    }
    // Check the existing entity is a directory we can access.
    struct stat s{};
    if (stat(path, &s)) {
        std::stringstream error_builder;
        error_builder << "Failed to check directory " << path
                      << " state with errno " << errno;
        log_and_throw_runtime_exception(env, error_builder.str().c_str());
        return;
    }
    if (!S_ISDIR(s.st_mode)) {
        std::stringstream error_builder;
        error_builder << "Path " << path << " exists and it is not a directory";
        log_and_throw_runtime_exception(env, error_builder.str().c_str());
        return;
    }
    if (access(path, R_OK | W_OK)) {
        std::stringstream error_builder;
        error_builder << "Directory " << path << " accessibility check failed with errno "
                      << errno;
        log_and_throw_runtime_exception(env, error_builder.str().c_str());
        return;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_syncTaskDir(JNIEnv *env, jobject, jstring path) {
    _info_log(TAG, "initialize: sync and create task info directories path");
    const char *value = env->GetStringUTFChars(path, nullptr);
    task_state_dir = ({
        std::stringstream builder;
        builder << value << "/ts";
        builder.str();
    });
    task_error_dir = ({
        std::stringstream builder;
        builder << value << "/err";
        builder.str();
    });
    env->ReleaseStringUTFChars(path, value);
    create_directory(env, task_state_dir.c_str());
    create_directory(env, task_error_dir.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_initializeSymbol(JNIEnv *env, jobject) {
    _info_log(TAG, "initialize: initialize symbol");
    if (!initialize_symbols()) {
        log_and_throw_runtime_exception(env, "Failed to initialize symbol");
        return;
    }
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

static void on_error(const char *error) {
    _error_log(TAG, "error happened: %s", error);
    update_task_error(error);
}

// ! execute on task process
static void execute_dump(const char *file_name) {
    _info_log(TAG, "task_process %d: dump", getpid());
    update_task_state(TS_DUMP);
    dump_heap(file_name);
}

// ! execute on task process
static void analyzer_error_listener(const char *message) {
    on_error(message);
}

// ! execute on task process
static std::optional<std::vector<LeakChain>>
execute_analyze(const char *hprof_path, const char *reference_key) {
    _info_log(TAG, "task_process %d: analyze", getpid());

    update_task_state(TS_ANALYZER_CREATE);
    const int hprof_fd = open(hprof_path, O_RDONLY);
    if (hprof_fd == -1) {
        std::stringstream error_builder;
        error_builder << "invoke open() failed on HPROF with errno " << errno;
        on_error(error_builder.str().c_str());
        return std::nullopt;
    }
    HprofAnalyzer::SetErrorListener(analyzer_error_listener);
    HprofAnalyzer analyzer(hprof_fd);

    update_task_state(TS_ANALYZER_INITIALIZE);
    if (!exclude_default_references(analyzer)) {
        on_error("exclude default references rules failed");
        return std::nullopt;
    }

    update_task_state(TS_ANALYZER_EXECUTE);
    return analyzer.Analyze([reference_key](const HprofHeap &heap) {
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
}

// ! execute on task process
static bool execute_serialize(const char *result_path, const std::vector<LeakChain> &leak_chains) {
    _info_log(TAG, "task_process %d: serialize", getpid());

    update_task_state(TS_CREATE_RESULT_FILE);
    bool result = false;
    int result_fd = open(result_path, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR);
    if (result_fd == -1) {
        std::stringstream error_builder;
        error_builder << "invoke open() failed on result file with errno " << errno;
        on_error(error_builder.str().c_str());
        return false;
    }

    update_task_state(TS_SERIALIZE);
    // See comment documentation of <code>MemoryUtil.deserialize</code> for the file format of
    // result file.
#define write_data(content, size)                                                           \
        if (write(result_fd, content, size) == -1) {                                        \
            std::stringstream error_builder;                                                \
            error_builder << "invoke write() failed on result file with errno " << errno;   \
            on_error(error_builder.str().c_str());                                          \
            goto write_leak_chain_done;                                                     \
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
        _error_log(TAG, "invoke waitpid failed with errno %d", errno);
        return create_task_result(env, TR_TYPE_WAIT_FAILED, errno, TS_UNKNOWN, "none");
    }

    const int8_t task_state = get_task_state_and_cleanup(pid);
    const std::string task_error = get_task_error_and_cleanup(pid);
    if (WIFEXITED(status)) {
        return create_task_result(env, TR_TYPE_EXIT, WEXITSTATUS(status), task_state, task_error);
    } else if (WIFSIGNALED(status)) {
        return create_task_result(env, TR_TYPE_SIGNALED, WTERMSIG(status), task_state, task_error);
    } else {
        return create_task_result(env, TR_TYPE_UNKNOWN, 0, task_state, task_error);
    }
}