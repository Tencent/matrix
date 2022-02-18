/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef pthread_introspection_h
#define pthread_introspection_h

#include <mach/mach.h>
#include <pthread/pthread.h>

#define STACK_CACHE_SIZE 253

struct pthread_stack_info {
    uint64_t stack_cache[STACK_CACHE_SIZE];
    pthread_t p_self;
    uintptr_t stacktop;
    uintptr_t stackbot;
};

static_assert(sizeof(pthread_stack_info) == (1 << 11), "Not aligned!");

// maybe fail
void memory_logging_pthread_introspection_hook_install();

pthread_stack_info *memory_logging_pthread_stack_info();
bool memory_logging_pthread_stack_exist(pthread_stack_info *stack_info, uint64_t hash);
void memory_logging_pthread_stack_remove(pthread_stack_info *stack_info, uint64_t hash);

#endif /* pthread_introspection_h */
