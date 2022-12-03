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

#undef LOGD
#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)

#ifdef __cplusplus
extern "C" {
#endif

#define HOOK_REGISTER(regex, target_sym, target_so) \
    do {                                            \
        FETCH_ORIGIN_FUNC_OF_SO(target_sym, target_so); \
        if(!ORIGINAL_FUNC_NAME(target_sym)) {       \
             LOGD(TAG, "hook failed: fetch origin func failed: %s", #target_sym);                                       \
             break;                                       \
        } \
        int ret = xhook_grouped_register(HOOK_REQUEST_GROUPID_MEMORY, regex, #target_sym, (void*) HANDLER_FUNC_NAME(target_sym), nullptr); \
        LOGD(TAG, "hook fn, regex: %s, sym: %s, ret: %d", regex, #target_sym, ret);                                                         \
         \
    } while(0);

#define HOOK_REGISTER_REGEX_LIBC(target_sym) \
    HOOK_REGISTER(regex, target_sym, "libc.so")

#define HOOK_REGISTER_REGEX_LIBCXX(target_sym) \
    HOOK_REGISTER(regex, target_sym, "libc++_shared.so")


bool enable_mmap_hook = false;

static void hook(const char *regex) {
    HOOK_REGISTER_REGEX_LIBC(malloc)
    HOOK_REGISTER_REGEX_LIBC(calloc)
    HOOK_REGISTER_REGEX_LIBC(realloc)
    HOOK_REGISTER_REGEX_LIBC(free)

    HOOK_REGISTER_REGEX_LIBC(memalign)
    HOOK_REGISTER_REGEX_LIBC(posix_memalign)
    HOOK_REGISTER_REGEX_LIBC(strdup)
    HOOK_REGISTER_REGEX_LIBC(strndup)

    // CXX functions
#ifndef __LP64__
    HOOK_REGISTER_REGEX_LIBCXX(_Znwj)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnwjSt11align_val_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnwjSt11align_val_tRKSt9nothrow_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnwjRKSt9nothrow_t)

    HOOK_REGISTER_REGEX_LIBCXX(_Znaj)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnajSt11align_val_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnajSt11align_val_tRKSt9nothrow_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnajRKSt9nothrow_t)

    HOOK_REGISTER_REGEX_LIBCXX(_ZdlPvj)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdlPvjSt11align_val_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdaPvj)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdaPvjSt11align_val_t)
#else
    HOOK_REGISTER_REGEX_LIBCXX(_Znwm)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnwmSt11align_val_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnwmSt11align_val_tRKSt9nothrow_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnwmRKSt9nothrow_t)

    HOOK_REGISTER_REGEX_LIBCXX(_Znam)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnamSt11align_val_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnamSt11align_val_tRKSt9nothrow_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZnamRKSt9nothrow_t)

    HOOK_REGISTER_REGEX_LIBCXX(_ZdlPvm)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdlPvmSt11align_val_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdaPvm)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdaPvmSt11align_val_t)
#endif
    HOOK_REGISTER_REGEX_LIBCXX(_ZdlPv)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdlPvSt11align_val_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdlPvSt11align_val_tRKSt9nothrow_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdlPvRKSt9nothrow_t)

    HOOK_REGISTER_REGEX_LIBCXX(_ZdaPv)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdaPvSt11align_val_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdaPvSt11align_val_tRKSt9nothrow_t)
    HOOK_REGISTER_REGEX_LIBCXX(_ZdaPvRKSt9nothrow_t)

    LOGD(TAG, "mmap enabled ? %d", enable_mmap_hook);
    if (enable_mmap_hook) {
        HOOK_REGISTER_REGEX_LIBC(mmap)
        HOOK_REGISTER_REGEX_LIBC(munmap)
        HOOK_REGISTER_REGEX_LIBC(mremap)
#if __ANDROID_API__ >= __ANDROID_API_L__
        HOOK_REGISTER_REGEX_LIBC(mmap64)
#endif
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
Java_com_tencent_matrix_hook_memory_MemoryHook_installHooksNative(JNIEnv* env, jobject thiz,
                                                                  jobjectArray hook_so_patterns,
                                                                  jobjectArray ignore_so_patterns,
                                                                  jboolean enable_debug) {
    memory_hook_init();
    LOGI(TAG, "memory_hook_init");

    xhook_block_refresh();

    {
        jsize size = env->GetArrayLength(hook_so_patterns);
        for (int i = 0; i < size; ++i) {
            auto jregex = (jstring) env->GetObjectArrayElement(hook_so_patterns, i);
            const char* regex = env->GetStringUTFChars(jregex, nullptr);
            hook(regex);
            env->ReleaseStringUTFChars(jregex, regex);
        }
    }

    if (ignore_so_patterns != nullptr) {
        jsize size = env->GetArrayLength(ignore_so_patterns);
        for (int i = 0; i < size; ++i) {
            auto jregex = (jstring) env->GetObjectArrayElement(ignore_so_patterns, i);
            const char* regex = env->GetStringUTFChars(jregex, nullptr);
            ignore(regex);
            env->ReleaseStringUTFChars(jregex, regex);
        }
    }

    NOTIFY_COMMON_IGNORE_LIBS(HOOK_REQUEST_GROUPID_MEMORY);

    // log@memoryhook -> log_impl@liblog -> __emutls_get_address@libandroid_runtime
    // -> calloc@memoryhook -> log@memoryhook
    // dead loop !!
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_MEMORY, ".*/libandroid_runtime\\.so$", nullptr);

    xhook_unblock_refresh();
}

#ifdef __cplusplus
}
#endif