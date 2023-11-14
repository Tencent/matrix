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
// Created by Yves on 2020/7/15.
//

#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <asm/mman.h>
#include <sys/mman.h>
#include "EnhanceDlsym.h"
#include "MallLog.h"
#include <stdatomic.h>

#define TAG "Matrix.JeCtl"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*je_mallctl_t)(const char *name,
                            void *oldp,
                            size_t *oldlenp,
                            void *newp,
                            size_t newlen);

typedef void *(*je_arena_extent_alloc_large_t)(void *tsdn,
                                               void *arena,
                                               size_t usize,
                                               size_t alignment,
                                               bool *zero);

typedef void (*je_large_dalloc_t)(void *tsdn, void *extent);

typedef void *(*je_arena_choose_hard_t)(void *tsd, bool internal);

typedef void (*je_arena_extent_dalloc_large_prep_t)(void *tsdn, void *arena, void *extent);

typedef int (*mallopt_t)(int param, int value);

#define MAX_RETRY_TIMES 10

void *handle     = nullptr;
bool initialized = false;

je_mallctl_t                        je_mallctl                        = nullptr;
je_arena_extent_alloc_large_t       je_arena_extent_alloc_large       = nullptr;
je_large_dalloc_t                   je_large_dalloc                   = nullptr;
je_arena_choose_hard_t              je_arena_choose_hard              = nullptr;
je_arena_extent_dalloc_large_prep_t je_arena_extent_dalloc_large_prep = nullptr;

bool *je_opt_retain_ptr = nullptr;

mallopt_t libc_mallopt = nullptr;

static bool init() {

    handle = enhance::dlopen("libc.so", 0);

    if (handle == nullptr) {
        return false;
    }

    libc_mallopt = (mallopt_t) enhance::dlsym(handle, "mallopt");

    je_mallctl      = (je_mallctl_t) enhance::dlsym(handle, "je_mallctl");

    if (!je_mallctl) {
        return false;
    }

    const char *version;
    size_t     size = sizeof(version);
    je_mallctl("version", &version, &size, nullptr, 0);
    LOGD(TAG, "jemalloc version: %s", version);

    je_opt_retain_ptr = (bool *) enhance::dlsym(handle, "je_opt_retain");
    if (!je_opt_retain_ptr) {
        return false;
    }

    return true;
}

static void flush_decay_purge() {
    assert(je_mallctl != nullptr);
    je_mallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
    je_mallctl("arena.0.decay", nullptr, nullptr, nullptr, 0);
    je_mallctl("arena.1.decay", nullptr, nullptr, nullptr, 0);
    je_mallctl("arena.0.purge", nullptr, nullptr, nullptr, 0);
    je_mallctl("arena.1.purge", nullptr, nullptr, nullptr, 0);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_mallctl_MallCtl_initNative(JNIEnv *env, jclass clazz) {
    if (!initialized) {
        initialized = init();
    }
}

JNIEXPORT jstring JNICALL
Java_com_tencent_matrix_mallctl_MallCtl_getVersionNative(JNIEnv *env, jclass clazz) {
#ifdef __LP64__
    return env->NewStringUTF("64-bit");
#else
    if (!je_mallctl) {
        return env->NewStringUTF("Error");
    }

    const char *version;
    size_t     size = sizeof(version);
    je_mallctl("version", &version, &size, nullptr, 0);
    LOGD(TAG, "jemalloc version: %s", version);

    return env->NewStringUTF(version);
#endif
}

#define MALLOPT_SYM_NOT_FOUND -1

JNIEXPORT jint JNICALL
Java_com_tencent_matrix_mallctl_MallCtl_malloptNative(JNIEnv *env, jclass clazz) {
    if (!libc_mallopt) {
        return MALLOPT_SYM_NOT_FOUND;
    }
    // On success, mallopt() returns 1.  On error, it returns 0.
    int ret = libc_mallopt(M_PURGE, 0);
    if (ret == 0) {
        // try fallback je ctls
        flush_decay_purge();
    }
    return ret;
}

JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_mallctl_MallCtl_setRetainNative(JNIEnv *env, jclass clazz,
                                                        jboolean enable) {
#ifdef __LP64__
    return true;
#else
    bool old = true;
    if (initialized && je_opt_retain_ptr) {
        old = *je_opt_retain_ptr;
        *je_opt_retain_ptr = enable;
        LOGD(TAG, "retain = %d", *je_opt_retain_ptr);
    }
    return old;
#endif
}

JNIEXPORT jint JNICALL
Java_com_tencent_matrix_mallctl_MallCtl_flushReadOnlyFilePagesNative(JNIEnv *env, jclass clazz,
                                                                     jlong begin, jlong size) {

    return madvise((void *)begin, (size_t) size, MADV_DONTNEED);
}

#ifdef __cplusplus
}
#endif