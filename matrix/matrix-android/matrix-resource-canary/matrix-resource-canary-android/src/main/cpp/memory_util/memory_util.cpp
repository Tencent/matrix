#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <jni.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "excludes/excludes.h"
#include "log.h"
#include "symbol/symbol.h"

#include "matrix_hprof_analyzer.h"

#define LOG_TAG "Matrix.MemoryUtil"

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_initialize(JNIEnv *, jobject) {
    return initialize_symbols();
}

using namespace matrix::hprof;

#define unwrap_optional(optional, nullopt_action)   \
    ({                                              \
        const auto &result = optional;              \
        if (!result.has_value()) nullopt_action;    \
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

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_resource_MemoryUtil_analyzeInternal(JNIEnv *env, jobject,
                                                            jstring java_hprof_path,
                                                            jstring java_result_path,
                                                            jstring java_reference_key) {
    const char *hprof_path = env->GetStringUTFChars(java_hprof_path, nullptr);
    const int hprof_fd = open(hprof_path, O_RDONLY);
    env->ReleaseStringUTFChars(java_hprof_path, hprof_path);
    if (hprof_fd == -1) {
        _error_log(LOG_TAG, "Failed to open HPROF file.");
        return false;
    }
    try {
        HprofAnalyzer analyzer(hprof_fd);
        if (!exclude_default_references(analyzer)) {
            return false;
        }

        const char *reference_key = env->GetStringUTFChars(java_reference_key, nullptr);
        auto leak_chains = analyzer.Analyze([reference_key](const HprofHeap &heap) {
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
        env->ReleaseStringUTFChars(java_reference_key, reference_key);

        const char *result_path = env->GetStringUTFChars(java_result_path, nullptr);
        bool result = false;
        int result_fd = open(result_path, O_WRONLY | O_CREAT | O_TRUNC);

        env->ReleaseStringUTFChars(java_result_path, result_path);
        if (result_fd == -1) {
            _error_log(LOG_TAG, "Failed to write leak chains to result path, errno: %d.", errno);
            return false;
        }
#define write_data(content, size)                                                           \
        if (write(result_fd, content, size) == -1) {                                    \
            _error_log(LOG_TAG, "Failed to write content to result path, errno: %d.",   \
                       errno);                                                          \
            goto write_leak_chain_done;                                                 \
        }

        const uint32_t leak_chain_count = leak_chains.size();
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
                write_data(&serialized_reference_type, sizeof(int32_t));
                const uint32_t reference_name_length = node.GetReference().length();
                write_data(&reference_name_length, sizeof(uint32_t))
                write_data(node.GetReference().c_str(), reference_name_length)

                const int32_t serialized_object_type =
                        serialize_object_type(node.GetObjectType());
                write_data(&serialized_object_type, sizeof(int32_t));
                const uint32_t object_name_length = node.GetObject().length();
                write_data(&object_name_length, sizeof(uint32_t));
                write_data(node.GetObject().c_str(), object_name_length);
            }

            const uint32_t end_tag = 0;
            write_data(&end_tag, sizeof(uint32_t))
        }
        result = true;
        write_leak_chain_done:
        close(result_fd);
        return result;
    } catch (const std::exception &) {
        return false;
    }
}

static void fork_process_crash_handler(int signal_num) {
    // Do nothing, just tell parent process to handle the error.
    exit(-3);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tencent_matrix_resource_MemoryUtilKt_fork(JNIEnv *, jclass, jlong child_timeout) {
    auto *thread = current_thread();
    suspend_runtime(thread);
    int pid = fork();
    if (pid == 0) {
        signal(SIGSEGV, fork_process_crash_handler);
        if (child_timeout != 0) alarm(child_timeout);
        prctl(PR_SET_NAME, "matrix_dump_process");
    } else {
        resume_runtime(thread);
    }
    return pid;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tencent_matrix_resource_MemoryUtilKt_wait(JNIEnv *, jclass, jint pid) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        _error_log(LOG_TAG, "Failed to invoke waitpid().");
        return -2;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else {
        _error_log(LOG_TAG, "Fork process exited unexpectedly.");
        return -2;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_tencent_matrix_resource_MemoryUtilKt_exit(JNIEnv *, jclass, jint code) {
    _exit(code);
}

extern "C" JNIEXPORT void JNICALL
Java_com_tencent_matrix_resource_MemoryUtilKt_free(JNIEnv *, jclass, jlong pointer) {
    free(reinterpret_cast<void *>(pointer));
}