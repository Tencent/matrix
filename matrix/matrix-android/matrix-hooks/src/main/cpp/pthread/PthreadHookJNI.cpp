//
// Created by Yves on 2020-03-11.
//

#include <jni.h>
#include <xhook.h>
#include <common/Log.h>
#include <common/ScopedCleaner.h>
#include "PthreadHook.h"
#include "ThreadTrace.h"
#include "ThreadStackShink.h"

using namespace pthread_hook;
using namespace thread_trace;
using namespace thread_stack_shink;

#define LOG_TAG "Matrix.PthreadHookJNI"

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_setThreadTraceEnabledNative(JNIEnv *env, jobject thiz, jboolean enabled) {
    pthread_hook::SetThreadTraceEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_setThreadStackShrinkEnabledNative(JNIEnv *env, jobject thiz, jboolean enabled) {
    pthread_hook::SetThreadStackShrinkEnabled(enabled);
}

JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_setThreadStackShrinkIgnoredCreatorSoPatternsNative(JNIEnv *env, jobject thiz, jobjectArray j_patterns) {
    if (j_patterns == nullptr) {
        LOGW(LOG_TAG, "nullptr was past as patterns, clear previous set patterns.");
        pthread_hook::SetThreadStackShrinkIgnoredCreatorSoPatterns(nullptr, 0);
        return true;
    }
    jsize patternCount = env->GetArrayLength(j_patterns);
    if (patternCount == 0) {
        LOGW(LOG_TAG, "Zero-length array was past as patterns, clear previous set patterns.");
        pthread_hook::SetThreadStackShrinkIgnoredCreatorSoPatterns(nullptr, 0);
        return true;
    }
    const char** patterns = reinterpret_cast<const char**>(::malloc(sizeof(const char*) * patternCount));
    if (patterns == nullptr) {
        LOGE(LOG_TAG, "Fail to allocate buffer to transfer java pattern string.");
        return false;
    }
    auto patternsCleaner = matrix::MakeScopedCleaner([&patterns]() {
        free(patterns);
    });
    for (int i = 0; i < patternCount; ++i) {
        jstring jPattern = reinterpret_cast<jstring>(env->GetObjectArrayElement(j_patterns, i));
        auto jPatternCleaner = matrix::MakeScopedCleaner([&env, &jPattern]() {
            env->DeleteLocalRef(jPattern);
        });
        patterns[i] = env->GetStringUTFChars(jPattern, nullptr);
    }
    pthread_hook::SetThreadStackShrinkIgnoredCreatorSoPatterns(patterns, patternCount);
    for (int i = 0; i < patternCount; ++i) {
        jstring jPattern = reinterpret_cast<jstring>(env->GetObjectArrayElement(j_patterns, i));
        auto jPatternCleaner = matrix::MakeScopedCleaner([&env, &jPattern]() {
            env->DeleteLocalRef(jPattern);
        });
        env->ReleaseStringUTFChars(jPattern, patterns[i]);
    }
    return true;
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_addHookThreadNameNative(JNIEnv *env, jobject thiz, jobjectArray thread_names) {
    jsize size = env->GetArrayLength(thread_names);

    for (int i = 0; i < size; ++i) {
        auto       jregex = (jstring) (env->GetObjectArrayElement(thread_names, i));
        const char *regex = env->GetStringUTFChars(jregex, NULL);
        add_hook_thread_name(regex);
        env->ReleaseStringUTFChars(jregex, regex);
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_dumpNative(JNIEnv *env, jobject thiz, jstring jpath) {
    if (jpath) {
        const char *path = env->GetStringUTFChars(jpath, NULL);
        pthread_dump_json(path);
        env->ReleaseStringUTFChars(jpath, path);
    } else {
        pthread_dump_json();
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_enableQuickenNative(JNIEnv *env, jobject thiz, jboolean enable) {
    enable_quicken_unwind(enable);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_enableLoggerNative(JNIEnv *, jclass, jboolean enable) {
    enable_hook_logger(enable);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_installHooksNative(JNIEnv *env, jobject thiz, jboolean enable_debug) {
    NOTIFY_COMMON_IGNORE_LIBS();
    xhook_ignore(".*/libmatrix-pthreadhook\\.so$", nullptr);

    pthread_hook::InstallHooks(enable_debug);
}

#ifdef __cplusplus
}
#endif