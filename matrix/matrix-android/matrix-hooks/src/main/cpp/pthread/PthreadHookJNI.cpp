/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created by Yves on 2020-03-11.
//

#include <jni.h>
#include <xhook.h>
#include "PthreadHook.h"
#include "Log.h"

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
Java_com_tencent_matrix_hook_pthread_PthreadHook_addHookSoNative(JNIEnv *env, jobject thiz,
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
Java_com_tencent_matrix_hook_pthread_PthreadHook_addIgnoreSoNative(JNIEnv *env, jobject thiz,
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
Java_com_tencent_matrix_hook_pthread_PthreadHook_addHookThreadNameNative(JNIEnv *env,
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
Java_com_tencent_matrix_hook_pthread_PthreadHook_dumpNative(JNIEnv *env, jobject thiz,
                                                                   jstring jpath) {
    if (jpath) {
        const char *path = env->GetStringUTFChars(jpath, NULL);
        pthread_dump_json(path);
        env->ReleaseStringUTFChars(jpath, path);
    } else {
        pthread_dump_json();
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_enableQuickenNative(JNIEnv *env, jobject thiz,
                                                            jboolean enable) {
    enable_quicken_unwind(enable);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_pthread_PthreadHook_enableLoggerNative(
        JNIEnv *env, jclass clazz, jboolean enable) {
    (void) env;
    (void) clazz;
    enable_hook_logger(enable);
}

#ifdef __cplusplus
}
#endif