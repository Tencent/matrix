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
// Created by Yves on 2020-03-16.
//
#include <QuickenUnwinder.h>
#include <QuickenJNI.h>
#include "JNICommon.h"
#include "Log.h"
#include "HookCommon.h"
#include "xhook.h"

#define TAG "Matrix.JNICommon"

#ifdef __cplusplus
extern "C" {
#endif

JavaVM *m_java_vm;

jclass m_class_HookManager;
jmethodID m_method_getStack;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD(TAG, "JNI OnLoad...");
    m_java_vm = vm;

    JNIEnv *env;

    vm->GetEnv((void **) &env, JNI_VERSION_1_6);

    if (env) {
        jclass j_HookManager = env->FindClass("com/tencent/matrix/hook/HookManager");

        if (j_HookManager) {
            LOGD(TAG, "j_PthreadHook not null");
            m_class_HookManager = (jclass) env->NewGlobalRef(j_HookManager);
            m_method_getStack = env->GetStaticMethodID(m_class_HookManager, "getStack",
                                                       "()Ljava/lang/String;");
        } else {
            LOGD(TAG, "j_PthreadHook null!");
        }
    }

    xhook_ignore(".*libmatrix-hooks\\.so$", NULL);
    xhook_ignore(".*librabbiteye\\.so$", NULL);
    xhook_ignore(".*libwechatbacktrace\\.so$", NULL);
    xhook_ignore(".*libwechatcrash\\.so$", NULL);
    xhook_ignore(".*libmemguard\\.so$", NULL);
    xhook_ignore(".*liblog\\.so$", NULL);
    xhook_ignore(".*libwechatxlog\\.so$", NULL);
    xhook_ignore(".*libc\\.so$", NULL);
    xhook_ignore(".*libm\\.so$", NULL);
    xhook_ignore(".*libc\\+\\+\\.so$", NULL);
    xhook_ignore(".*libc\\+\\+_shared\\.so$", NULL);
    xhook_ignore(".*libstdc\\+\\+.so\\.so$", NULL);
    xhook_ignore(".*libstlport_shared\\.so$", NULL);

    xhook_register(".*\\.so$", "__loader_android_dlopen_ext",
                   (void *) HANDLER_FUNC_NAME(__loader_android_dlopen_ext),
                   (void **) &ORIGINAL_FUNC_NAME(__loader_android_dlopen_ext));


    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {

}

#ifdef __cplusplus
}
#endif

#undef TAG