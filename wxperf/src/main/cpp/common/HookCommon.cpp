//
// Created by Yves on 2020-03-17.
//

#include <jni.h>
#include <xhook.h>
#include <sys/stat.h>
#include "HookCommon.h"
#include "Log.h"
#include "StackTrace.h"
#include "JNICommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAG "HookCommon"

std::vector<dlopen_callback_t> m_dlopen_callbacks;
std::vector<hook_init_callback_t> m_init_callbacks;

static pthread_mutex_t m_dlopen_mutex;

DEFINE_HOOK_FUN(void *, __loader_android_dlopen_ext, const char *__file_name,
                int                                             __flag,
                const void                                      *__extinfo,
                const void                                      *__caller_addr) {
    void *ret = (*ORIGINAL_FUNC_NAME(__loader_android_dlopen_ext))(__file_name, __flag, __extinfo,
                                                                   __caller_addr);

    LOGD(TAG, "call into dlopen hook");
    pthread_mutex_lock(&m_dlopen_mutex);

    for (auto &callback : m_dlopen_callbacks) {
        callback(__file_name);
    }

    NanoSeconds_Start(begin);
    xhook_refresh(false);
    NanoSeconds_End(cost, begin);

    LOGD(TAG, "xhook_refresh cost : %lld", cost);

    pthread_mutex_unlock(&m_dlopen_mutex);
    return ret;
}

static void hook_common_init() {
    for (auto &callback : m_init_callbacks) {
        callback();
    }
}

void add_dlopen_hook_callback(dlopen_callback_t __callback) {
    m_dlopen_callbacks.push_back(__callback);
}

void add_hook_init_callback(hook_init_callback_t __callback) {
    m_init_callbacks.push_back(__callback);
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

bool get_java_stacktrace(char *__stack, size_t __size) {
    JNIEnv *env = nullptr;

    if (!__stack) {
        return false;
    }

    if (m_java_vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_OK) {
        LOGD(TAG, "get_java_stacktrace call");

        jstring j_stacktrace = (jstring) env->CallStaticObjectMethod(m_class_HookManager, m_method_getStack);

        LOGD(TAG, "get_java_stacktrace called");
        const char *stack = env->GetStringUTFChars(j_stacktrace, NULL);
        if (stack) {
            const size_t cpy_len = std::min(strlen(stack) + 1, __size - 1);
            memcpy(__stack, stack, cpy_len);
            __stack[cpy_len] = '\0';
        } else {
            strncpy(__stack, "\tget java stacktrace failed", __size);
        }
        env->ReleaseStringUTFChars(j_stacktrace, stack);

        return true;
    }

    strncpy(__stack, "\tnull", __size);
    return false;
}

JNIEXPORT jint JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookRefreshNative(JNIEnv *env, jobject thiz,
                                                                  jboolean async) {
    hook_common_init();
    unwindstack::update_maps();
    NanoSeconds_Start(begin);
    int ret = xhook_refresh(async);
    NanoSeconds_End(cost, begin);

    LOGD(TAG, "xhook_refresh in JNI cost %lld", cost);
    return ret;
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookEnableDebugNative(JNIEnv *env, jobject thiz,
                                                                      jboolean flag) {
    xhook_enable_debug(flag);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookEnableSigSegvProtectionNative(JNIEnv *env,
                                                                                  jobject thiz,
                                                                                  jboolean flag) {
    xhook_enable_sigsegv_protection(flag);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookClearNative(JNIEnv *env, jobject thiz) {
    xhook_clear();
}

#ifdef __cplusplus
}

#undef TAG

#endif