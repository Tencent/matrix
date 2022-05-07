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

#include "pthread_introspection.h"
#include "logger_internal.h"

#include <pthread/introspection.h>

#pragma mark -
#pragma mark Constants/Globals

// We set malloc_logger to NULL to disable logging, if we encounter errors
// during file writing
typedef void(malloc_logger_t)(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t num_hot_frames_to_skip);
extern malloc_logger_t __memory_event_callback;

static bool s_hook_success = false;

static pthread_key_t s_thread_stack_info_key = 0;
static pthread_introspection_hook_t s_old_pthread_introspection_hook = NULL;

#pragma mark -
#pragma mark Functions

pthread_stack_info *__pthread_stack_info_init(void) {
    pthread_stack_info *stack_info = (pthread_stack_info *)inter_malloc(sizeof(pthread_stack_info));
    stack_info->p_self = pthread_self();
    stack_info->stacktop = (uintptr_t)pthread_get_stackaddr_np(stack_info->p_self);
    stack_info->stackbot = stack_info->stacktop - pthread_get_stacksize_np(stack_info->p_self);
    pthread_setspecific(s_thread_stack_info_key, stack_info);
    return stack_info;
}

void __pthread_stack_info_free(void *ptr) {
    inter_free(ptr);
}

void memory_logging_pthread_introspection_hook(unsigned int event, pthread_t thread, void *addr, size_t size) {
    switch (event) {
        case PTHREAD_INTROSPECTION_THREAD_CREATE:
            // do nothing
            break;
        case PTHREAD_INTROSPECTION_THREAD_START: {
            __pthread_stack_info_init();
            break;
        }
        case PTHREAD_INTROSPECTION_THREAD_TERMINATE: {
            //
            __memory_event_callback(memory_logging_type_vm_deallocate, 0, (uintptr_t)addr, size, 0, 0);
            break;
        }
        case PTHREAD_INTROSPECTION_THREAD_DESTROY:
            // do nothing
            break;
        default:
            break;
    }

    // callback last hook
    if (s_old_pthread_introspection_hook) {
        s_old_pthread_introspection_hook(event, thread, addr, size);
    }
}

void memory_logging_pthread_introspection_hook_install() {
    s_old_pthread_introspection_hook = pthread_introspection_hook_install(memory_logging_pthread_introspection_hook);
    if (pthread_key_create(&s_thread_stack_info_key, __pthread_stack_info_free) != 0) {
        return;
    }

    // main thread
    __pthread_stack_info_init();
    s_hook_success = true;
}

pthread_stack_info *memory_logging_pthread_stack_info() {
    if (s_hook_success) {
        pthread_stack_info *stack_info = (pthread_stack_info *)pthread_getspecific(s_thread_stack_info_key);
        return stack_info;
    } else {
        return NULL;
    }
}

bool memory_logging_pthread_stack_exist(pthread_stack_info *stack_info, uint64_t hash) {
    if (stack_info) {
        if (stack_info->stack_cache[hash % STACK_CACHE_SIZE] == hash) {
            return true;
        } else {
            stack_info->stack_cache[hash % STACK_CACHE_SIZE] = hash;
            return false;
        }
    } else {
        return false;
    }
}

void memory_logging_pthread_stack_remove(pthread_stack_info *stack_info, uint64_t hash) {
    if (stack_info && hash) {
        stack_info->stack_cache[hash % STACK_CACHE_SIZE] = 0;
    }
}
