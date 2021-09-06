/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "memory_logging_event_buffer_pool.h"

#include <dispatch/dispatch.h>

#define BUFFER_SIZE (2 << 10)
#define MAX_BUFFER_COUNT (4 << 10)

#pragma mark -
#pragma mark Types

struct memory_logging_event_buffer_pool {
    malloc_lock_s lock;
    dispatch_semaphore_t pool_semaphore;

    int max_buffer_count;
    int buffer_size;

    void *pool_memory;
    memory_logging_event_buffer *curr_buffer;
    memory_logging_event_buffer *tail_buffer;
};

extern int dump_call_stacks;
extern thread_id s_main_thread_id;

static_assert(sizeof(memory_logging_event_buffer) % 8 == 0, "Not aligned!");

#pragma mark -
#pragma mark Functions

memory_logging_event_buffer_pool *memory_logging_event_buffer_pool_create() {
    memory_logging_event_buffer_pool *buffer_pool = (memory_logging_event_buffer_pool *)inter_malloc(sizeof(memory_logging_event_buffer_pool));
    if (buffer_pool == NULL) {
        return NULL;
    }

    buffer_pool->max_buffer_count = MAX_BUFFER_COUNT;
    buffer_pool->buffer_size = (dump_call_stacks == 0 ? (BUFFER_SIZE / 4) : BUFFER_SIZE);

    buffer_pool->lock = __malloc_lock_init();
    buffer_pool->pool_semaphore = dispatch_semaphore_create(buffer_pool->max_buffer_count);
    buffer_pool->pool_memory = inter_malloc(buffer_pool->max_buffer_count * buffer_pool->buffer_size);
    if (buffer_pool->pool_memory == NULL) {
        memory_logging_event_buffer_pool_free(buffer_pool);
        return NULL;
    }

    memory_logging_event_buffer *last_buffer = NULL;
    for (int i = 0; i < buffer_pool->max_buffer_count; ++i) {
        memory_logging_event_buffer *curr_buffer = (memory_logging_event_buffer *)((char *)buffer_pool->pool_memory + i * buffer_pool->buffer_size);
        curr_buffer->is_from_buffer_pool = true;
        curr_buffer->lock = __malloc_lock_init();
        curr_buffer->buffer = (uint8_t *)curr_buffer + sizeof(memory_logging_event_buffer); // % 8 = 0
        curr_buffer->buffer_size = buffer_pool->buffer_size - sizeof(memory_logging_event_buffer);

        if (last_buffer) {
            last_buffer->next_event_buffer = curr_buffer;
        }
        last_buffer = curr_buffer;
    }
    last_buffer->next_event_buffer = NULL;

    buffer_pool->curr_buffer = (memory_logging_event_buffer *)buffer_pool->pool_memory;
    buffer_pool->tail_buffer = last_buffer;

    return buffer_pool;
}

void memory_logging_event_buffer_pool_free(memory_logging_event_buffer_pool *buffer_pool) {
    if (buffer_pool == NULL) {
        return;
    }

    // avoid that after the memory monitoring stops, there are still some events being written
    while (dispatch_semaphore_wait(buffer_pool->pool_semaphore, DISPATCH_TIME_NOW) != 0) {
        buffer_pool->curr_buffer = (memory_logging_event_buffer *)buffer_pool->pool_memory;
        dispatch_semaphore_signal(buffer_pool->pool_semaphore);
    }

    inter_free(buffer_pool->pool_memory);
    inter_free(buffer_pool);
}

memory_logging_event_buffer *memory_logging_event_buffer_pool_new_buffer(memory_logging_event_buffer_pool *buffer_pool, thread_id t_id) {
    memory_logging_event_buffer *event_buffer;

    if (t_id == s_main_thread_id) {
        if (dispatch_semaphore_wait(buffer_pool->pool_semaphore, DISPATCH_TIME_NOW) != 0) {
            event_buffer = (memory_logging_event_buffer *)inter_malloc(buffer_pool->buffer_size);
            event_buffer->is_from_buffer_pool = false;
            event_buffer->lock = __malloc_lock_init();
            event_buffer->buffer = (uint8_t *)event_buffer + sizeof(memory_logging_event_buffer); // % 8 = 0
            event_buffer->buffer_size = buffer_pool->buffer_size - sizeof(memory_logging_event_buffer);
        } else {
            __malloc_lock_lock(&buffer_pool->lock);

            event_buffer = buffer_pool->curr_buffer;
            buffer_pool->curr_buffer = event_buffer->next_event_buffer;

            __malloc_lock_unlock(&buffer_pool->lock);
        }
    } else {
        dispatch_semaphore_wait(buffer_pool->pool_semaphore, DISPATCH_TIME_FOREVER);

        __malloc_lock_lock(&buffer_pool->lock);

        event_buffer = buffer_pool->curr_buffer;
        buffer_pool->curr_buffer = event_buffer->next_event_buffer;

        __malloc_lock_unlock(&buffer_pool->lock);
    }

    event_buffer->write_index = 0;
    event_buffer->last_write_index = 0;
    event_buffer->t_id = t_id;
    event_buffer->next_event_buffer = NULL;

    return event_buffer;
}

bool memory_logging_event_buffer_pool_free_buffer(memory_logging_event_buffer_pool *buffer_pool, memory_logging_event_buffer *event_buffer) {
    if (event_buffer->is_from_buffer_pool == false) {
        inter_free(event_buffer);
        return false;
    }

    __malloc_lock_lock(&buffer_pool->lock);

    if (buffer_pool->curr_buffer == NULL) {
        buffer_pool->curr_buffer = event_buffer;
        buffer_pool->tail_buffer = event_buffer;
    } else {
        buffer_pool->tail_buffer->next_event_buffer = event_buffer;
        buffer_pool->tail_buffer = event_buffer;
    }

    event_buffer->next_event_buffer = NULL;

    __malloc_lock_unlock(&buffer_pool->lock);

    return dispatch_semaphore_signal(buffer_pool->pool_semaphore) != 0;
}
