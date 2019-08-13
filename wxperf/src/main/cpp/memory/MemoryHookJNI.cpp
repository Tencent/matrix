//
// Created by Yves on 2019-08-08.
//
#include <jni.h>
#include "xhook.h"
#include "MemoryHook.h"
#include "StackTrace.h"

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_enableStacktraceNative(JNIEnv *env,
                                                                             jclass type,
                                                                             jboolean enable) {

    enableStacktrace(enable);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookInitNative(JNIEnv *env, jclass type) {
    xhook_register(".*\\.so$", "malloc", (void *) h_malloc, NULL);
    xhook_register(".*\\.so$", "calloc", (void *) h_calloc, NULL);
    xhook_register(".*\\.so$", "realloc", (void *) h_realloc, NULL);
    xhook_register(".*\\.so$", "free", (void *) h_free, NULL);
//    xhook_register(".*\\.so$", "dlopen", (void *)h_dlopen, NULL);
    xhook_register(".*\\.so$", "__loader_android_dlopen_ext", (void *) h_dlopen, (void **)&p_oldfun);

    xhook_ignore(".*libwxperf\\.so$", NULL);
    xhook_ignore(".*liblog\\.so$", NULL);
    xhook_ignore(".*libc\\.so$", NULL);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookClearNative(JNIEnv *env, jclass type) {
    xhook_clear();
}

JNIEXPORT jint JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookRefreshNative(JNIEnv *env, jclass type,
                                                                         jboolean async) {
    return xhook_refresh(async);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookEnableDebugNative(JNIEnv *env,
                                                                             jclass type,
                                                                             jboolean flag) {
    xhook_enable_debug(flag);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookEnableSigSegvProtectionNative(
        JNIEnv *env, jclass type, jboolean flag) {
    xhook_enable_sigsegv_protection(flag);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_dump(JNIEnv *env, jclass type) {

    dump();

}

#ifdef __cplusplus
}
#endif