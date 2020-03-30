//
// Created by Yves on 2020-03-11.
//

#include <jni.h>
#include <xhook.h>
#include "PthreadHook.h"
#include "StackTrace.h"

#ifdef __cplusplus
extern "C" {
#endif

static HookFunction const HOOK_FUNCTIONS[] = {
        {"pthread_create",     (void *) HANDLER_FUNC_NAME(pthread_create),     NULL},
        {"pthread_setname_np", (void *) HANDLER_FUNC_NAME(pthread_setname_np), NULL}
};

static void hook_impl(const char *regex) {
    for (auto f: HOOK_FUNCTIONS) {
        xhook_register(regex, f.name, f.handler_ptr, f.origin_ptr);
    }
}

static void ignore_impl(const char *regex) {
    for (auto f: HOOK_FUNCTIONS) {
        xhook_ignore(regex, f.name);
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_pthread_PthreadHook_addHookSoNative(JNIEnv *env, jobject thiz,
                                                                        jobjectArray hook_so_list) {
    jsize size = env->GetArrayLength(hook_so_list);

    for (int i = 0; i < size; ++i) {
        auto       jregex = (jstring) (env->GetObjectArrayElement(hook_so_list, i));
        const char *regex = env->GetStringUTFChars(jregex, NULL);
        hook_impl(regex);
        env->ReleaseStringUTFChars(jregex, regex);
    }

    add_dlopen_hook_callback(pthread_hook_on_dlopen);
    add_hook_init_callback(pthread_hook_init);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_pthread_PthreadHook_addIgnoreSoNative(JNIEnv *env, jobject thiz,
                                                                          jobjectArray hook_so_list) {
    jsize size = env->GetArrayLength(hook_so_list);

    for (int i = 0; i < size; ++i) {
        auto       jregex = (jstring) (env->GetObjectArrayElement(hook_so_list, i));
        const char *regex = env->GetStringUTFChars(jregex, NULL);
        ignore_impl(regex);
        env->ReleaseStringUTFChars(jregex, regex);
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_pthread_PthreadHook_addHookThreadNameNative(JNIEnv *env,
                                                                                jobject thiz,
                                                                                jobjectArray thread_names) {
    jsize size = env->GetArrayLength(thread_names);

    for (int i = 0; i < size; ++i) {
        auto       jregex = (jstring) (env->GetObjectArrayElement(thread_names, i));
        const char *regex = env->GetStringUTFChars(jregex, NULL);
        add_hook_thread_name(regex);
        env->ReleaseStringUTFChars(jregex, regex);
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_pthread_PthreadHook_dumpNative(JNIEnv *env, jobject thiz,
                                                                   jstring jpath) {
    if (jpath) {
        const char *path = env->GetStringUTFChars(jpath, NULL);
        pthread_dump(path);
        env->ReleaseStringUTFChars(jpath, path);
    } else {
        pthread_dump();
    }
}


#ifdef __cplusplus
}
#endif