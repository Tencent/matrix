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
// Created by Yves on 2019-08-08.
//
#include <jni.h>
#include <xhook.h>
#include <xhook_ext.h>
#include <xh_errno.h>
#include <common/HookCommon.h>
#include <common/SoLoadMonitor.h>
#include "MemoryHookFunctions.h"
#include "MemoryHook.h"

#ifdef __cplusplus
extern "C" {
#endif
// @formatter:off
const HookFunction HOOK_MALL_FUNCTIONS[] = {
        {"malloc", (void *) h_malloc, NULL},
        {"calloc", (void *) h_calloc, NULL},
        {"realloc", (void *) h_realloc, NULL},
        {"free", (void *) h_free, NULL},
        {"memalign", (void *) HANDLER_FUNC_NAME(memalign), NULL},
        {"posix_memalign", (void *) HANDLER_FUNC_NAME(posix_memalign), NULL},
        // CXX functions
#ifndef __LP64__
        {"_Znwj",                               (void*) HANDLER_FUNC_NAME(_Znwj), NULL},
        {"_ZnwjSt11align_val_t",                (void*) HANDLER_FUNC_NAME(_ZnwjSt11align_val_t), NULL},
        {"_ZnwjSt11align_val_tRKSt9nothrow_t",  (void*) HANDLER_FUNC_NAME(_ZnwjSt11align_val_tRKSt9nothrow_t), NULL},
        {"_ZnwjRKSt9nothrow_t",                 (void*) HANDLER_FUNC_NAME(_ZnwjRKSt9nothrow_t), NULL},

        {"_Znaj",                               (void*) HANDLER_FUNC_NAME(_Znaj), NULL},
        {"_ZnajSt11align_val_t",                (void*) HANDLER_FUNC_NAME(_ZnajSt11align_val_t), NULL},
        {"_ZnajSt11align_val_tRKSt9nothrow_t",  (void*) HANDLER_FUNC_NAME(_ZnajSt11align_val_tRKSt9nothrow_t), NULL},
        {"_ZnajRKSt9nothrow_t",                 (void*) HANDLER_FUNC_NAME(_ZnajRKSt9nothrow_t), NULL},

        {"_ZdlPvj",                             (void*) HANDLER_FUNC_NAME(_ZdlPvj), NULL},
        {"_ZdlPvjSt11align_val_t",              (void*) HANDLER_FUNC_NAME(_ZdlPvjSt11align_val_t), NULL},
        {"_ZdaPvj",                             (void*) HANDLER_FUNC_NAME(_ZdaPvj), NULL},
        {"_ZdaPvjSt11align_val_t",              (void*) HANDLER_FUNC_NAME(_ZdaPvjSt11align_val_t), NULL},
#else
        {"_Znwm",                               (void*) HANDLER_FUNC_NAME(_Znwm), NULL},
        {"_ZnwmSt11align_val_t",                (void*) HANDLER_FUNC_NAME(_ZnwmSt11align_val_t), NULL},
        {"_ZnwmSt11align_val_tRKSt9nothrow_t",  (void*) HANDLER_FUNC_NAME(_ZnwmSt11align_val_tRKSt9nothrow_t), NULL},
        {"_ZnwmRKSt9nothrow_t",                 (void*) HANDLER_FUNC_NAME(_ZnwmRKSt9nothrow_t), NULL},

        {"_Znam",                               (void*) HANDLER_FUNC_NAME(_Znam), NULL},
        {"_ZnamSt11align_val_t",                (void*) HANDLER_FUNC_NAME(_ZnamSt11align_val_t), NULL},
        {"_ZnamSt11align_val_tRKSt9nothrow_t",  (void*) HANDLER_FUNC_NAME(_ZnamSt11align_val_tRKSt9nothrow_t), NULL},
        {"_ZnamRKSt9nothrow_t",                 (void*) HANDLER_FUNC_NAME(_ZnamRKSt9nothrow_t), NULL},

        {"_ZdlPvm",                             (void*) HANDLER_FUNC_NAME(_ZdlPvm), NULL},
        {"_ZdlPvmSt11align_val_t",              (void*) HANDLER_FUNC_NAME(_ZdlPvmSt11align_val_t), NULL},
        {"_ZdaPvm",                             (void*) HANDLER_FUNC_NAME(_ZdaPvm), NULL},
        {"_ZdaPvmSt11align_val_t",              (void*) HANDLER_FUNC_NAME(_ZdaPvmSt11align_val_t), NULL},
#endif
        {"_ZdlPv",                              (void*) HANDLER_FUNC_NAME(_ZdlPv), NULL},
        {"_ZdlPvSt11align_val_t",               (void*) HANDLER_FUNC_NAME(_ZdlPvSt11align_val_t), NULL},
        {"_ZdlPvSt11align_val_tRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZdlPvSt11align_val_tRKSt9nothrow_t), NULL},
        {"_ZdlPvRKSt9nothrow_t",                (void*) HANDLER_FUNC_NAME(_ZdlPvRKSt9nothrow_t), NULL},

        {"_ZdaPv",                              (void*) HANDLER_FUNC_NAME(_ZdaPv), NULL},
        {"_ZdaPvSt11align_val_t",               (void*) HANDLER_FUNC_NAME(_ZdaPvSt11align_val_t), NULL},
        {"_ZdaPvSt11align_val_tRKSt9nothrow_t", (void*) HANDLER_FUNC_NAME(_ZdaPvSt11align_val_tRKSt9nothrow_t), NULL},
        {"_ZdaPvRKSt9nothrow_t",                (void*) HANDLER_FUNC_NAME(_ZdaPvRKSt9nothrow_t), NULL},

        {"strdup", (void*) HANDLER_FUNC_NAME(strdup), (void **) ORIGINAL_FUNC_NAME(strdup)},
        {"strndup", (void*) HANDLER_FUNC_NAME(strndup), (void **) ORIGINAL_FUNC_NAME(strndup)},
};

static const HookFunction HOOK_MMAP_FUNCTIONS[] = {
        {"mmap", (void *) h_mmap, NULL},
        {"munmap", (void *) h_munmap, NULL},
        {"mremap", (void *) h_mremap, NULL},
#if __ANDROID_API__ >= __ANDROID_API_L__
        {"mmap64", (void *) h_mmap64, NULL},
#endif
};
// @formatter:on

bool enable_mmap_hook = false;

static void hook(const char *regex) {

    for (auto f : HOOK_MALL_FUNCTIONS) {
        int ret = xhook_grouped_register(HOOK_REQUEST_GROUPID_MEMORY, regex, f.name, f.handler_ptr, f.origin_ptr);
        LOGD(TAG, "hook fn, regex: %s, sym: %s, ret: %d", regex, f.name, ret);
    }
    LOGD(TAG, "mmap enabled ? %d", enable_mmap_hook);
    if (enable_mmap_hook) {
        for (auto f: HOOK_MMAP_FUNCTIONS) {
            xhook_grouped_register(HOOK_REQUEST_GROUPID_MEMORY, regex, f.name, f.handler_ptr, f.origin_ptr);
        }
    }
}

static void ignore(const char *regex) {
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_MEMORY, regex, nullptr);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_memory_MemoryHook_enableStacktraceNative(JNIEnv *env,
                                                                     jobject instance,
                                                                     jboolean enable) {

    enable_stacktrace(enable);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_memory_MemoryHook_addHookSoNative(JNIEnv *env, jobject instance,
                                                              jobjectArray hookSoList) {

    jsize size = env->GetArrayLength(hookSoList);

    for (int i = 0; i < size; ++i) {
        auto       jregex = (jstring) env->GetObjectArrayElement(hookSoList, i);
        const char *regex = env->GetStringUTFChars(jregex, NULL);
        hook(regex);
        env->ReleaseStringUTFChars(jregex, regex);
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_memory_MemoryHook_addIgnoreSoNative(JNIEnv *env,
                                                                jobject instance,
                                                                jobjectArray ignoreSoList) {

    if (!ignoreSoList) {
        return;
    }

    jsize size = env->GetArrayLength(ignoreSoList);

    for (int i = 0; i < size; ++i) {
        auto       jregex = (jstring) env->GetObjectArrayElement(ignoreSoList, i);
        const char *regex = env->GetStringUTFChars(jregex, NULL);
        ignore(regex);
        env->ReleaseStringUTFChars(jregex, regex);
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_memory_MemoryHook_setTracingAllocSizeRangeNative(JNIEnv *env,
                                                                              jobject instance,
                                                                              jint minSize,
                                                                              jint maxSize) {

    set_tracing_alloc_size_range((size_t) minSize, (size_t) maxSize);

}
JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_memory_MemoryHook_dumpNative(JNIEnv *env, jobject instance,
                                                         jstring j_log_path, jstring j_json_path) {

    const char *log_path  = j_log_path ? env->GetStringUTFChars(j_log_path, nullptr) : nullptr;
    const char *json_path = j_json_path ? env->GetStringUTFChars(j_json_path, nullptr) : nullptr;

    dump(enable_mmap_hook, log_path, json_path);

    if (j_log_path) {
        env->ReleaseStringUTFChars(j_log_path, log_path);
    }
    if (j_json_path) {
        env->ReleaseStringUTFChars(j_json_path, json_path);
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_memory_MemoryHook_enableMmapHookNative(JNIEnv *env,
                                                                   jobject instance,
                                                                   jboolean enable) {

    LOGD("Yves.debug", "jni enableMmapHookNative %d", enable);
    enable_mmap_hook = enable;

}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_memory_MemoryHook_setStacktraceLogThresholdNative(JNIEnv *env,
                                                                              jobject thiz,
                                                                              jint threshold) {
    assert(threshold >= 0);
    set_stacktrace_log_threshold(threshold);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_memory_MemoryHook_installHooksNative(JNIEnv* env, jobject thiz, jboolean enable_debug) {
    memory_hook_init();
    LOGI(TAG, "memory_hook_init");

    NOTIFY_COMMON_IGNORE_LIBS(HOOK_REQUEST_GROUPID_MEMORY);

    // log@memoryhook -> log_impl@liblog -> __emutls_get_address@libandroid_runtime
    // -> calloc@memoryhook -> log@memoryhook
    // dead loop !!
    xhook_ignore(".*/libandroid_runtime\\.so$", nullptr);
}

#ifdef __cplusplus
}
#endif