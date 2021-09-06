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
#include <backtrace/QuickenUnwinder.h>
#include <xh_errno.h>
#include <backtrace/common/PthreadExt.h>
#include <backtrace/Backtrace.h>
#include "JNICommon.h"
#include "Log.h"
#include "HookCommon.h"
#include "ScopedCleaner.h"
#include "xhook.h"
#include "ReentrantPrevention.h"
#include "SoLoadMonitor.h"

#define TAG "Matrix.JNICommon"

#ifdef __cplusplus
extern "C" {
#endif

JavaVM *m_java_vm;

static volatile bool s_prehook_initialized = false;
static std::mutex s_prehook_init_mutex;
static volatile bool s_finalook_initialized = false;
static std::mutex s_finalhook_init_mutex;
jclass m_class_HookManager;
jmethodID m_method_getStack;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD(TAG, "JNI OnLoad...");
    m_java_vm = vm;
    return JNI_VERSION_1_6;
}

static jclass FindClass(JNIEnv* env, const char* desc, bool ret_global_ref = false) {
    jclass clazz = env->FindClass(desc);
    if (clazz == nullptr) {
        env->ExceptionClear();
        LOGE(TAG, "Cannot find class: %s", desc);
    }
    if (ret_global_ref) {
        clazz = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
        if (clazz == nullptr) {
            LOGE(TAG, "Cannot create global ref for class: %s", desc);
        }
    }
    return clazz;
}

static jmethodID GetStaticMethodID(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    jmethodID mid = env->GetStaticMethodID(clazz, name, sig);
    if (mid == nullptr) {
        env->ExceptionClear();
        LOGE(TAG, "Fail to get static method id of %s:%s", name, sig);
    }
    return mid;
}

JNIEXPORT jboolean Java_com_tencent_matrix_hook_HookManager_doPreHookInitializeNative(JNIEnv *env, jobject, jboolean /* debug */) {
    std::lock_guard prehookInitLock(s_prehook_init_mutex);

    if (s_prehook_initialized) {
        LOGE(TAG, "doPreHookInitializeNative was already called.");
        return true;
    }

    m_class_HookManager = FindClass(env, "com/tencent/matrix/hook/HookManager", true);
    if (m_class_HookManager == nullptr) {
        return false;
    }
    m_class_HookManager = reinterpret_cast<jclass>(env->NewGlobalRef(m_class_HookManager));
    auto jHookMgrCleaner = matrix::MakeScopedCleaner([env]() {
        if (m_class_HookManager != nullptr) {
            env->DeleteGlobalRef(m_class_HookManager);
            m_class_HookManager = nullptr;
        }
    });

    m_method_getStack = GetStaticMethodID(env, m_class_HookManager, "getStack", "()Ljava/lang/String;");
    if (m_method_getStack == nullptr) {
        return false;
    }
    auto getStackMethodCleaner = matrix::MakeScopedCleaner([]() {
        m_method_getStack = nullptr;
    });

    if (!matrix::InstallSoLoadMonitor()) {
        return false;
    }

    pthread_ext_init();
    rp_init();

    getStackMethodCleaner.Omit();
    jHookMgrCleaner.Omit();

    s_prehook_initialized = true;
    return true;
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_HookManager_doFinalInitializeNative(JNIEnv *env, jobject thiz, jboolean debug) {
    std::lock_guard finalInitLock(s_finalhook_init_mutex);

    if (s_finalook_initialized) {
        LOGE(TAG, "doFinalInitializeNative was already called.");
        return;
    }

    wechat_backtrace::notify_maps_changed();

    xhook_enable_debug(debug ? 1 : 0);
    xhook_enable_sigsegv_protection(debug ? 0 : 1);

    int ret = xhook_refresh(0);
    if (ret != 0) {
        LOGE(TAG, "Fail to call xhook_refresh, ret: %d", ret);
    }

    s_finalook_initialized = true;
}

#ifdef __cplusplus
}
#endif

#undef TAG