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

#ifndef LIBMATRIX_HOOK_THREADTRACE_H
#define LIBMATRIX_HOOK_THREADTRACE_H


#include <pthread.h>
#include <common/HookCommon.h>
#include "PthreadHook.h"


namespace thread_trace {
    void thread_trace_init();

    void add_hook_thread_name(const char *regex_str);

    void pthread_dump_json(const char *path);

    void enable_quicken_unwind(const bool enable);

    void enable_trace_pthread_release(const bool enable);

    typedef struct {
        pthread_hook::pthread_routine_t wrapped_func;
        pthread_hook::pthread_routine_t origin_func;
        void *origin_args;
    } routine_wrapper_t;

    routine_wrapper_t* wrap_pthread_routine(pthread_hook::pthread_routine_t start_routine, void* args);

    void handle_pthread_create(const pthread_t pthread);

    void handle_pthread_setname_np(pthread_t pthread, const char* name);

    void handle_pthread_release(pthread_t pthread);
}


#endif //LIBMATRIX_HOOK_THREADTRACE_H
