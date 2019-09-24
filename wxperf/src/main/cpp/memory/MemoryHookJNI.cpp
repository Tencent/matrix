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

static void registerCXXFun(const char *lib_pattern) {
#ifndef __LP64__

    xhook_register(lib_pattern, "_Znwj", (void*) HANDLER_FUNC_NAME(_Znwj), NULL);
    xhook_register(lib_pattern, "_ZnwjSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZnwjSt11align_val_t), NULL);
    xhook_register(lib_pattern, "_ZnwjSt11align_val_tRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZnwjSt11align_val_tRKSt9nothrow_t), NULL);
    xhook_register(lib_pattern, "_ZnwjRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZnwjRKSt9nothrow_t), NULL);
    xhook_register(lib_pattern, "_Znaj", (void*) HANDLER_FUNC_NAME(_Znaj), NULL);
    xhook_register(lib_pattern, "_ZnajSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZnajSt11align_val_t), NULL);
    xhook_register(lib_pattern, "_ZnajSt11align_val_tRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZnajSt11align_val_tRKSt9nothrow_t), NULL);
    xhook_register(lib_pattern, "_ZnajRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZnajRKSt9nothrow_t), NULL);

    xhook_register(lib_pattern, "_ZdlPvj", (void*) HANDLER_FUNC_NAME(_ZdlPvj), NULL);
    xhook_register(lib_pattern, "_ZdlPvjSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZdlPvjSt11align_val_t), NULL);

    xhook_register(lib_pattern, "_ZdaPvj", (void*) HANDLER_FUNC_NAME(_ZdaPvj), NULL);
    xhook_register(lib_pattern, "_ZdaPvjSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZdaPvjSt11align_val_t), NULL);
#else

    xhook_register(lib_pattern, "_Znwm", (void*) HANDLER_FUNC_NAME(_Znwm), NULL);
    xhook_register(lib_pattern, "_ZnwmSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZnwmSt11align_val_t), NULL);
    xhook_register(lib_pattern, "_ZnwmSt11align_val_tRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZnwmSt11align_val_tRKSt9nothrow_t), NULL);
    xhook_register(lib_pattern, "_ZnwmRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZnwmRKSt9nothrow_t), NULL);
    xhook_register(lib_pattern, "_Znam", (void*) HANDLER_FUNC_NAME(_Znam), NULL);
    xhook_register(lib_pattern, "_ZnamSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZnamSt11align_val_t), NULL);
    xhook_register(lib_pattern, "_ZnamSt11align_val_tRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZnamSt11align_val_tRKSt9nothrow_t), NULL);
    xhook_register(lib_pattern, "_ZnamRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZnamRKSt9nothrow_t), NULL);

    xhook_register(lib_pattern, "_ZdlPvm", (void*) HANDLER_FUNC_NAME(_ZdlPvm), NULL);
    xhook_register(lib_pattern, "_ZdlPvmSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZdlPvmSt11align_val_t), NULL);

    xhook_register(lib_pattern, "_ZdaPvm", (void*) HANDLER_FUNC_NAME(_ZdaPvm), NULL);
    xhook_register(lib_pattern, "_ZdaPvmSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZdaPvmSt11align_val_t), NULL);
#endif

    xhook_register(lib_pattern, "_ZdlPv", (void*) HANDLER_FUNC_NAME(_ZdlPv), NULL);
    xhook_register(lib_pattern, "_ZdlPvSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZdlPvSt11align_val_t), NULL);
    xhook_register(lib_pattern, "_ZdlPvSt11align_val_tRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZdlPvSt11align_val_tRKSt9nothrow_t), NULL);
    xhook_register(lib_pattern, "_ZdlPvRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZdlPvRKSt9nothrow_t), NULL);

    xhook_register(lib_pattern, "_ZdaPv", (void*) HANDLER_FUNC_NAME(_ZdaPv), NULL);
    xhook_register(lib_pattern, "_ZdaPvSt11align_val_t", (void*) HANDLER_FUNC_NAME(_ZdaPvSt11align_val_t), NULL);
    xhook_register(lib_pattern, "_ZdaPvSt11align_val_tRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZdaPvSt11align_val_tRKSt9nothrow_t), NULL);
    xhook_register(lib_pattern, "_ZdaPvRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZdaPvRKSt9nothrow_t), NULL);

    xhook_register(lib_pattern, "strdup", (void*) HANDLER_FUNC_NAME(strdup), NULL);
    xhook_register(lib_pattern, "strndup", (void*) HANDLER_FUNC_NAME(strndup), NULL);
}

static void hook(const char *regex) {
    xhook_register(regex, "malloc", (void *) h_malloc, NULL);
    xhook_register(regex, "calloc", (void *) h_calloc, NULL);
    xhook_register(regex, "realloc", (void *) h_realloc, NULL);
    xhook_register(regex, "free", (void *) h_free, NULL);
    registerCXXFun(regex);
}

static void ignore(const char *regex) {
    xhook_ignore(regex, NULL);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_enableStacktraceNative(JNIEnv *env,
                                                                             jobject instance,
                                                                             jboolean enable) {

    enableStacktrace(enable);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookRegisterNative(JNIEnv *env, jobject instance,
                                                                          jobjectArray hookSoList) {

    jsize size = env->GetArrayLength(hookSoList);

    for (int i = 0; i < size; ++i) {
        auto jregex = (jstring) env->GetObjectArrayElement(hookSoList, i);
        const char *regex = env->GetStringUTFChars(jregex, 0);
        hook(regex);
        env->ReleaseStringUTFChars(jregex, regex);
    }

    // fixme move to init
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookIgnoreNative(JNIEnv *env, jobject instance,
                                                                        jobjectArray ignoreSoList) {

    if (!ignoreSoList) {
        return;
    }

    jsize size = env->GetArrayLength(ignoreSoList);

    for (int i = 0; i < size; ++i) {
        auto jregex = (jstring) env->GetObjectArrayElement(ignoreSoList, i);
        const char *regex = env->GetStringUTFChars(jregex, 0);
        ignore(regex);
        env->ReleaseStringUTFChars(jregex, regex);
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookInitNative(JNIEnv *env, jobject instance) {
//    xhook_register(".*\\.so$", "malloc", (void *) h_malloc, NULL);
//    xhook_register(".*\\.so$", "calloc", (void *) h_calloc, NULL);
//    xhook_register(".*\\.so$", "realloc", (void *) h_realloc, NULL);
//    xhook_register(".*\\.so$", "free", (void *) h_free, NULL);
//    xhook_register(".*\\.so$", "dlopen", (void *)h_dlopen, NULL);
//    xhook_register(".*\\.so$", "__loader_android_dlopen_ext", (void *) h_dlopen,
//                   (void **) &p_oldfun);
//
//    xhook_ignore(".*libwxperf\\.so$", NULL);
//    xhook_ignore(".*liblog\\.so$", NULL);
//    xhook_ignore(".*libc\\.so$", NULL);
//    xhook_ignore(".*libc++_shared\\.so$", NULL);

    xhook_ignore(".*libwxperf\\.so$", NULL);
    xhook_ignore(".*liblog\\.so$", NULL);
    xhook_ignore(".*libc\\.so$", NULL);
//    xhook_ignore(".*libc\\+\\+\\.so$", NULL);
    xhook_ignore(".*libc\\+\\+_shared\\.so$", NULL);
    xhook_ignore(".*libstlport_shared\\.so$", NULL);
    xhook_ignore(".*libm\\.so$", NULL);
    //    xhook_ignore(".*libart\\.so$", NULL);

    xhook_register(".*\\.so$", "__loader_android_dlopen_ext", (void *) h_dlopen,
                   (void **) &orig_dlopen);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookClearNative(JNIEnv *env, jobject instance) {
    xhook_clear();
}

JNIEXPORT jint JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookRefreshNative(JNIEnv *env, jobject instance,
                                                                         jboolean async) {
    unwindstack::update_maps();
    return xhook_refresh(async);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookEnableDebugNative(JNIEnv *env,
                                                                             jobject instance,
                                                                             jboolean flag) {
    xhook_enable_debug(flag);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_xhookEnableSigSegvProtectionNative(
        JNIEnv *env, jobject instance, jboolean flag) {
    xhook_enable_sigsegv_protection(flag);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_dump(JNIEnv *env, jobject instance) {

//    dump();

}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_groupByMemorySize(JNIEnv *env, jobject instance,
                                                                        jboolean enable) {

    enableGroupBySize(enable);

}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_setSamplingNative(JNIEnv *env, jobject instance,
                                                                        jdouble sampling) {

    setSampling(sampling);

}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_setSampleSizeRangeNative(JNIEnv *env,
                                                                               jobject instance,
                                                                               jint minSize,
                                                                               jint maxSize) {

    setSampleSizeRange((size_t)minSize, (size_t)maxSize);

}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_memory_MemoryHook_dumpNative(JNIEnv *env, jobject instance,
                                                                 jstring jpath_) {

    if (jpath_) {
        const char *path = env->GetStringUTFChars(jpath_, 0);
        dump(path);
        env->ReleaseStringUTFChars(jpath_, path);
    } else {
        dump();
    }
}

#ifdef __cplusplus
}
#endif