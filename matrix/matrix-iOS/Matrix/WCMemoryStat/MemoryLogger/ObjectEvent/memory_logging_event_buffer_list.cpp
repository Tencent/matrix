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

#include "memory_logging_event_buffer_list.h"

#pragma mark -
#pragma mark Types

struct memory_logging_event_buffer_list {
    malloc_lock_s lock;

    memory_logging_event_buffer *curr_buffer;
    memory_logging_event_buffer *tail_buffer;
};

#pragma mark -
#pragma mark Functions

memory_logging_event_buffer_list *memory_logging_event_buffer_list_create() {
    memory_logging_event_buffer_list *buffer_list = (memory_logging_event_buffer_list *)inter_malloc(sizeof(memory_logging_event_buffer_list));

    buffer_list->lock = __malloc_lock_init();
    buffer_list->curr_buffer = NULL;
    buffer_list->tail_buffer = NULL;

    return buffer_list;
}

void memory_logging_event_buffer_list_free(memory_logging_event_buffer_list *buffer_list) {
    if (buffer_list == NULL) {
        return;
    }

    inter_free(buffer_list);
}

void memory_logging_event_buffer_list_push_back(memory_logging_event_buffer_list *buffer_list, memory_logging_event_buffer *event_buffer) {
    __malloc_lock_lock(&buffer_list->lock);

    if (buffer_list->curr_buffer == NULL) {
        buffer_list->curr_buffer = event_buffer;
        buffer_list->tail_buffer = event_buffer;
    } else {
        buffer_list->tail_buffer->next_event_buffer = event_buffer;
        buffer_list->tail_buffer = event_buffer;
    }

    event_buffer->next_event_buffer = NULL;

    __malloc_lock_unlock(&buffer_list->lock);
}

memory_logging_event_buffer *memory_logging_event_buffer_list_pop_all(memory_logging_event_buffer_list *buffer_list) {
    __malloc_lock_lock(&buffer_list->lock);

    memory_logging_event_buffer *curr_buffer = buffer_list->curr_buffer;
    if (curr_buffer) {
        buffer_list->curr_buffer = NULL;
    }

    __malloc_lock_unlock(&buffer_list->lock);

    return curr_buffer;
}
