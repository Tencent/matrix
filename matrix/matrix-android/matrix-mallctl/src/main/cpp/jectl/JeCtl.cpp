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
#include "JeLog.h"
#include <stdatomic.h>

#define TAG "Matrix.JeCtl"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*mallctl_t)(const char *name,
                         void *oldp,
                         size_t *oldlenp,
                         void *newp,
                         size_t newlen);

typedef void *(*arena_extent_alloc_large_t)(void *tsdn,
                                            void *arena,
                                            size_t usize,
                                            size_t alignment,
                                            bool *zero);

typedef void (*large_dalloc_t)(void *tsdn, void *extent);

typedef void *(*arena_choose_hard_t)(void *tsd, bool internal);

typedef void (*arena_extent_dalloc_large_prep_t)(void *tsdn, void *arena, void *extent);

typedef void (*arena_extents_dirty_dalloc_t)(void *tsdn, void *arena,
                                             extent_hooks_t **r_extent_hooks, void *extent);

#define MAX_RETRY_TIMES 10

void *handle     = nullptr;
bool initialized = false;

mallctl_t                        mallctl                        = nullptr;
arena_extent_alloc_large_t       arena_extent_alloc_large       = nullptr;
large_dalloc_t                   large_dalloc                   = nullptr;
arena_choose_hard_t              arena_choose_hard              = nullptr;
arena_extent_dalloc_large_prep_t arena_extent_dalloc_large_prep = nullptr;
arena_extents_dirty_dalloc_t     arena_extents_dirty_dalloc     = nullptr;
bool * p_je_opt_retain = nullptr;

static bool init() {

    handle = enhance::dlopen("libc.so", 0);

    if (handle == nullptr) {
        return false;
    }

    mallctl         = (mallctl_t) enhance::dlsym(handle, "je_mallctl");

    if (!mallctl) {
        return false;
    }

    const char *version;
    size_t     size = sizeof(version);
    mallctl("version", &version, &size, nullptr, 0);
    LOGD(TAG, "jemalloc version: %s", version);

    if (0 != strncmp(version, "5.1.0", 5)) {
        return false;
    }

//    Dl_info mallctl_info{};
//    if (0 == dladdr((void *) mallctl, &mallctl_info)
//        || !end_with(mallctl_info.dli_fname, "/libc.so")) {
//        LOGD(TAG, "mallctl = %p, is a fault address, fname = %s, sname = %s", mallctl,
//             mallctl_info.dli_fname, mallctl_info.dli_sname);
//        mallctl = nullptr;
//        return false;
//    }

    p_je_opt_retain = (bool *) enhance::dlsym(handle, "je_opt_retain");
    if (!p_je_opt_retain) {
        return false;
    }

    arena_extent_alloc_large =
            (arena_extent_alloc_large_t) enhance::dlsym(handle, "je_arena_extent_alloc_large");
    if (!arena_extent_alloc_large) {
        return false;
    }

    arena_choose_hard = (arena_choose_hard_t) enhance::dlsym(handle, "je_arena_choose_hard");
    if (!arena_choose_hard) {
        return false;
    }

    large_dalloc = (large_dalloc_t) enhance::dlsym(handle, "je_large_dalloc");
    if (!large_dalloc) {
        return false;
    }

    arena_extent_dalloc_large_prep = (arena_extent_dalloc_large_prep_t) enhance::dlsym(handle,
                                                                                       "je_arena_extent_dalloc_large_prep");
    if (!arena_extent_dalloc_large_prep) {
        return false;
    }

    arena_extents_dirty_dalloc = (arena_extents_dirty_dalloc_t) enhance::dlsym(handle,
                                                                               "je_arena_extents_dirty_dalloc");
    if (!arena_extents_dirty_dalloc) {
        return false;
    }

    return true;
}

static void flush_decay_purge() {
    assert(mallctl != nullptr);
    mallctl("thread.tcache.flush", nullptr, nullptr, nullptr, 0);
    mallctl("arena.0.decay", nullptr, nullptr, nullptr, 0);
    mallctl("arena.1.decay", nullptr, nullptr, nullptr, 0);
    mallctl("arena.0.purge", nullptr, nullptr, nullptr, 0);
    mallctl("arena.1.purge", nullptr, nullptr, nullptr, 0);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_mallctl_MallCtl_initNative(JNIEnv *env, jclass clazz) {
#ifdef __LP64__
    return ;
#else
    if (!initialized) {
        initialized = init();
    }
#endif
}

JNIEXPORT jstring JNICALL
Java_com_tencent_matrix_mallctl_MallCtl_getVersionNative(JNIEnv *env, jclass clazz) {
#ifdef __LP64__
    return env->NewStringUTF("64-bit");
#else
    if (!mallctl) {
        return env->NewStringUTF("Error");
    }

    const char *version;
    size_t     size = sizeof(version);
    mallctl("version", &version, &size, nullptr, 0);
    LOGD(TAG, "jemalloc version: %s", version);

    return env->NewStringUTF(version);
#endif
}

#ifdef __cplusplus
}
#endif