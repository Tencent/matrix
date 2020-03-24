//
// Created by Yves on 2020-03-17.
//

#include <jni.h>
#include <xhook.h>
#include "HookCommon.h"
#include "log.h"
#include "StackTrace.h"
#include "JNICommon.h"

#ifdef __cplusplus
extern "C" {
#endif

std::vector<DLOPEN_CALLBACK> m_callbacks;

DEFINE_HOOK_FUN(void *, __loader_android_dlopen_ext, const char *__file_name,
                int                                             __flag,
                const void                                      *__extinfo,
                const void                                      *__caller_addr) {
    void *ret = (*ORIGINAL_FUNC_NAME(__loader_android_dlopen_ext))(__file_name, __flag, __extinfo,
                                                                   __caller_addr);

    LOGD("Yves-debug", "call into dlopen hook");

    for (auto &callback : m_callbacks) {
        callback(__file_name);
    }

    xhook_refresh(false);

    return ret;
}

void add_dlopen_hook_callback(DLOPEN_CALLBACK __callback) {
    m_callbacks.push_back(__callback);
}

bool get_java_stacktrace(char *__stack) {
    JNIEnv *env;
    if (m_java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK) {

        jstring j_stacktrace = (jstring) env->CallStaticObjectMethod(m_class_HookManager, m_method_getStack);
        const char *stack = env->GetStringUTFChars(j_stacktrace, NULL);
        strcpy(__stack, stack);
        LOGD("Yves-debug", "get_java_stacktrace: Java Stacktrace = \n%s", stack);
        env->ReleaseStringUTFChars(j_stacktrace, stack);
        return true;
    }
    return false;
}

JNIEXPORT jint JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookRefreshNative(JNIEnv *env, jobject thiz,
                                                                  jboolean async) {
    LOGD("Yves.debug", "refresh...");
    unwindstack::update_maps();
    return xhook_refresh(async);
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
#endif