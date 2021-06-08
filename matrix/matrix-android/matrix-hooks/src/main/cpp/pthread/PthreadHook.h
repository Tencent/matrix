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

#ifndef LIBMATRIX_HOOK_PTHREADHOOK_H
#define LIBMATRIX_HOOK_PTHREADHOOK_H

#include <pthread.h>
#include "HookCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

//void add_hook_parent_thread_name(const char *__regex_str);
void pthread_hook_init();

void add_hook_thread_name(const char *__regex_str);

void pthread_dump(const char *path = "/sdcard/pthread_hook.log");

void pthread_dump_json(const char *path = "/sdcard/pthread_hook.log");

void pthread_hook_on_dlopen(const char *file_name, bool *maps_refreshed);

void enable_quicken_unwind(const bool enable);

inline int wrap_pthread_getname_np(pthread_t pthread, char *buf, size_t n);

DECLARE_HOOK_ORIG(int, pthread_create, pthread_t* pthread_ptr, pthread_attr_t const* attr, void* (*start_routine)(void*), void* arg);

DECLARE_HOOK_ORIG(int, pthread_setname_np, pthread_t pthread, const char* name);

#ifdef __cplusplus
}
#endif

#endif //LIBMATRIX_HOOK_PTHREADHOOK_H
