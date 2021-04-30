//
// Created by Yves on 2020-03-11.
//

#include <jni.h>
#include <xhook.h>
#include "PthreadHook.h"
#include "ThreadTrace.h"
#include "ThreadStackShink.h"

using namespace pthread_hook;
using namespace thread_trace;
using namespace thread_stack_shink;

#ifdef __cplusplus
extern "C" {
#endif

// static void ignore_impl(const char *regex) {
//     for (auto f: HOOK_FUNCTIONS) {
//         xhook_ignore(regex, f.name);
//     }
// }

// JNIEXPORT void JNICALL
// Java_com_tencent_matrix_hook_pthread_PthreadHook_addHookSoNative(JNIEnv *env, jobject thiz,
//                                                                         jobjectArray hook_so_list) {
//     jsize size = env->GetArrayLength(hook_so_list);
//
//     for (int i = 0; i < size; ++i) {
//         auto       jregex = (jstring) (env->GetObjectArrayElement(hook_so_list, i));
//         const char *regex = env->GetStringUTFChars(jregex, NULL);
//         hook_impl(regex);
//         env->ReleaseStringUTFChars(jregex, regex);
//     }
//
//     add_dlopen_hook_callback(pthread_hook_on_dlopen);
//     add_hook_init_callback(thread_trace_init);
// }
//
// JNIEXPORT void JNICALL
// Java_com_tencent_matrix_hook_pthread_PthreadHook_addIgnoreSoNative(JNIEnv *env, jobject thiz,
//                                                                           jobjectArray hook_so_list) {
//     jsize size = env->GetArrayLength(hook_so_list);
//
//     for (int i = 0; i < size; ++i) {
//         auto       jregex = (jstring) (env->GetObjectArrayElement(hook_so_list, i));
//         const char *regex = env->GetStringUTFChars(jregex, NULL);
//         ignore_impl(regex);
//         env->ReleaseStringUTFChars(jregex, regex);
//     }
// }

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_setThreadTraceEnabledNative(JNIEnv *env, jobject thiz, jboolean enabled) {
    pthread_hook::SetThreadTraceEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_setThreadStackShinkEnabledNative(JNIEnv *env, jobject thiz, jboolean enabled) {
    pthread_hook::SetThreadStackShinkEnabled(enabled);
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
Java_com_tencent_matrix_hook_pthread_PthreadHook_installHooksNative(JNIEnv *env, jobject thiz) {
    pthread_hook::InstallHooks();
}

#ifdef __cplusplus
}
#endif