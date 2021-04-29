
// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// managed_jnienv.h

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
