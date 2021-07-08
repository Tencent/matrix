//
// Created by Yves on 2020-03-17.
//

#include <jni.h>
#include <sys/stat.h>
#include <backtrace/Backtrace.h>
#include <xhook.h>
#include "HookCommon.h"
#include "Log.h"
#include "JNICommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAG "Matrix.HookCommon"

std::vector<hook_init_callback_t> m_init_callbacks;

void add_hook_init_callback(hook_init_callback_t callback) {
    m_init_callbacks.push_back(callback);
}

void test_log_to_file(const char *ch) {
    const char *dir = "/sdcard/Android/data/com.tencent.mm/MicroMsg/Diagnostic";
    const char *path = "/sdcard/Android/data/com.tencent.mm/MicroMsg/Diagnostic/log";

    mkdir(dir, S_IRWXU|S_IRWXG|S_IROTH);

    FILE *log_file = fopen(path, "a+");

    if (!log_file) {
        return;
    }

    fprintf(log_file, "%p:%s\n", ch, ch);

    fflush(log_file);
    fclose(log_file);

}

bool get_java_stacktrace(char *stack_dst, size_t size) {
    JNIEnv *env = nullptr;

    if (!stack_dst) {
        return false;
    }

    if (m_java_vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_OK) {
        LOGD(TAG, "get_java_stacktrace call");

        jstring j_stacktrace = (jstring) env->CallStaticObjectMethod(m_class_HookManager, m_method_getStack);

        LOGD(TAG, "get_java_stacktrace called");
        const char *stacktrace = env->GetStringUTFChars(j_stacktrace, NULL);
        if (stacktrace) {
            const size_t stack_len = strlen(stacktrace);
            const size_t cpy_len = std::min(stack_len, size - 1);
            memcpy(stack_dst, stacktrace, cpy_len);
            stack_dst[cpy_len] = '\0';
        } else {
            strncpy(stack_dst, "\tget java stacktrace failed", size);
        }
        env->ReleaseStringUTFChars(j_stacktrace, stacktrace);
        env->DeleteLocalRef(j_stacktrace);
        return true;
    }

    strncpy(stack_dst, "\tnull", size);
    return false;
}

#ifdef __cplusplus
}

#undef TAG

#endif