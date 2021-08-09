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

#include "memory_logging_event_buffer.h"

#include <stdlib.h>

#pragma mark -
#pragma mark Functions

void memory_logging_event_buffer_compress(memory_logging_event_buffer *event_buffer) {
    memory_logging_event *curr_event = (memory_logging_event *)event_buffer->buffer;
    void *event_buffer_end = event_buffer->buffer + event_buffer->write_index;

    memory_logging_event *alloc_events[256];
    int alloc_event_count = 0;

    while (curr_event < event_buffer_end) {
        if (curr_event->event_type == EventType_Alloc) {
            alloc_events[alloc_event_count++] = curr_event;
        } else {
            uint64_t address = curr_event->address;

            for (int end = alloc_event_count - 1, i = end; i >= 0; --i) {
                memory_logging_event *last_alloc_event = alloc_events[i];
                if (last_alloc_event->address == address) {
                    if (last_alloc_event->type_flags & memory_logging_type_alloc) {
                        if (curr_event->type_flags & memory_logging_type_dealloc) {
                            // *waves hand* current allocation never occurred
                            last_alloc_event->event_type = EventType_Invalid;
                            curr_event->event_type = EventType_Invalid;

                            for (int j = i; j < end; ++j) {
                                alloc_events[j] = alloc_events[j + 1];
                            }
                            --alloc_event_count;
                            break;
                        } else if (curr_event->event_type == EventType_Update) {
                            if (curr_event->stack_size == 0) {
                                last_alloc_event->object_type = curr_event->object_type;
                                curr_event->event_type = EventType_Invalid;
                            } else {
                                curr_event->size = last_alloc_event->size;
                                curr_event->type_flags = last_alloc_event->type_flags;
                                curr_event->event_type = EventType_Alloc;
                                last_alloc_event->event_type = EventType_Invalid;
                                alloc_events[i] = curr_event;
                            }
                            break;
                        }
                    } else if (curr_event->type_flags & memory_logging_type_vm_deallocate) {
                        // *waves hand* current allocation never occurred
                        last_alloc_event->event_type = EventType_Invalid;
                        curr_event->event_type = EventType_Invalid;

                        for (int j = i; j < end; ++j) {
                            alloc_events[j] = alloc_events[j + 1];
                        }
                        --alloc_event_count;
                        break;
                    }
                }
            }
        }
        curr_event = (memory_logging_event *)((uint8_t *)curr_event + curr_event->event_size);
    }
}

void memory_logging_event_buffer_lock(memory_logging_event_buffer *event_buffer) {
    __malloc_lock_lock(&event_buffer->lock);
}

void memory_logging_event_buffer_unlock(memory_logging_event_buffer *event_buffer) {
    __malloc_lock_unlock(&event_buffer->lock);
}

bool memory_logging_event_buffer_is_full(memory_logging_event_buffer *event_buffer, bool is_dump_call_stacks) {
    if (is_dump_call_stacks) {
        return event_buffer->write_index > event_buffer->buffer_size - sizeof(memory_logging_event);
    } else {
        return event_buffer->write_index > event_buffer->buffer_size - MEMORY_LOGGING_EVENT_SIMPLE_SIZE;
    }
}

memory_logging_event *memory_logging_event_buffer_new_event(memory_logging_event_buffer *event_buffer) {
    return (memory_logging_event *)(event_buffer->buffer + event_buffer->write_index);
}

memory_logging_event *memory_logging_event_buffer_last_event(memory_logging_event_buffer *event_buffer) {
    if (event_buffer->last_write_index < event_buffer->write_index) {
        return (memory_logging_event *)(event_buffer->buffer + event_buffer->last_write_index);
    } else {
        return NULL;
    }
}

memory_logging_event *memory_logging_event_buffer_begin(memory_logging_event_buffer *event_buffer) {
    if (event_buffer->write_index == 0) {
        return NULL;
    }

    return (memory_logging_event *)event_buffer->buffer;
}

memory_logging_event *memory_logging_event_buffer_next(memory_logging_event_buffer *event_buffer, memory_logging_event *curr_event) {
    void *event_buffer_end = event_buffer->buffer + event_buffer->write_index;
    memory_logging_event *next_event = (memory_logging_event *)((uint8_t *)curr_event + curr_event->event_size);

    if (next_event < event_buffer_end) {
        return next_event;
    } else if (next_event == event_buffer_end) {
        return NULL;
    } else {
        disable_memory_logging();
        __malloc_printf("curr_event: %p, next_event: %p, write_index: %d, buffer_size: %u, self: %p, self->buffer: %p",
                        curr_event,
                        next_event,
                        event_buffer->write_index,
                        event_buffer->buffer_size,
                        event_buffer,
                        event_buffer->buffer);
        return NULL;
    }
}

void memory_logging_event_buffer_update_write_index_with_size(memory_logging_event_buffer *event_buffer, size_t write_size) {
    event_buffer->last_write_index = event_buffer->write_index;
    event_buffer->write_index += write_size;
}

void memory_logging_event_buffer_update_to_last_write_index(memory_logging_event_buffer *event_buffer) {
    event_buffer->write_index = event_buffer->last_write_index;
}
