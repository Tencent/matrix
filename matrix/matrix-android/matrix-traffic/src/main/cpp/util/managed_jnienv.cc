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

// Author: leafjia@tencent.com
//
// managed_jnienv.cc

#include "managed_jnienv.h"

#include <pthread.h>

#include <cstdlib>

namespace JniInvocation {

    static JavaVM *g_VM = nullptr;
    static pthread_once_t g_onceInitTls = PTHREAD_ONCE_INIT;
    static pthread_key_t g_tlsJavaEnv;

    void init(JavaVM *vm) {
        if (g_VM) return;
        g_VM = vm;
    }

    JavaVM *getJavaVM() {
        return g_VM;
    }

    JNIEnv *getEnv() {
        JNIEnv *env;
        int ret = g_VM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (ret != JNI_OK) {
            pthread_once(&g_onceInitTls, []() {
                pthread_key_create(&g_tlsJavaEnv, [](void *d) {
                    if (d && g_VM)
                        g_VM->DetachCurrentThread();
                });
            });

            if (g_VM->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                pthread_setspecific(g_tlsJavaEnv, reinterpret_cast<const void*>(1));
            } else {
                env = nullptr;
            }
        }
        return env;
    }

}   // namespace JniInvocation
