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

#define BUFFER_SIZE         (4 << 10)
#define MAX_BUFFER_COUNT    (512)

#pragma mark -
#pragma mark Types

struct memory_logging_event_buffer_pool {
    malloc_lock_s lock;
    dispatch_semaphore_t pool_semaphore;
    
    void *pool_memory;
    memory_logging_event_buffer *curr_buffer;
    memory_logging_event_buffer *tail_buffer;
};

#pragma mark -
#pragma mark Functions

memory_logging_event_buffer_pool *memory_logging_event_buffer_pool_create()
{
    memory_logging_event_buffer_pool *buffer_pool = (memory_logging_event_buffer_pool *)inter_malloc(sizeof(memory_logging_event_buffer_pool));
    if (buffer_pool == NULL) {
        return NULL;
    }
    
    buffer_pool->lock = __malloc_lock_init();
    buffer_pool->pool_semaphore = dispatch_semaphore_create(MAX_BUFFER_COUNT);
    buffer_pool->pool_memory = inter_malloc(MAX_BUFFER_COUNT * BUFFER_SIZE);
    if (buffer_pool->pool_memory == NULL) {
        memory_logging_event_buffer_pool_free(buffer_pool);
        return NULL;
    }
    
    memory_logging_event_buffer *last_buffer = NULL;
    for (int i = 0; i < MAX_BUFFER_COUNT; ++i) {
        memory_logging_event_buffer *curr_buffer = (memory_logging_event_buffer *)((char *)buffer_pool->pool_memory + i * BUFFER_SIZE);
        curr_buffer->lock = __malloc_lock_init();
        curr_buffer->buffer_size = BUFFER_SIZE - sizeof(memory_logging_event_buffer);
        curr_buffer->buffer = (uint8_t *)curr_buffer + sizeof(memory_logging_event_buffer); // % 8 = 0
        
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

void memory_logging_event_buffer_pool_free(memory_logging_event_buffer_pool *buffer_pool)
{
    if (buffer_pool == NULL) {
        return;
    }

    inter_free(buffer_pool->pool_memory);
    inter_free(buffer_pool);
}

memory_logging_event_buffer *memory_logging_event_buffer_pool_new_buffer(memory_logging_event_buffer_pool *buffer_pool)
{
    dispatch_semaphore_wait(buffer_pool->pool_semaphore, DISPATCH_TIME_FOREVER);
    
    __malloc_lock_lock(&buffer_pool->lock);
    
    memory_logging_event_buffer *event_buffer = buffer_pool->curr_buffer;
    buffer_pool->curr_buffer = event_buffer->next_event_buffer;

    __malloc_lock_unlock(&buffer_pool->lock);
    
    event_buffer->next_event_buffer = NULL;
    event_buffer->write_index = 0;
    event_buffer->last_write_index = 0;
    
    return event_buffer;
}

void memory_logging_event_buffer_pool_free_buffer(memory_logging_event_buffer_pool *buffer_pool, memory_logging_event_buffer *event_buffer)
{
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
    
    dispatch_semaphore_signal(buffer_pool->pool_semaphore);
}
