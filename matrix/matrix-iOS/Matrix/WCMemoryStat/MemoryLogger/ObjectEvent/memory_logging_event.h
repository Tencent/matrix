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

#ifndef memory_logging_event_h
#define memory_logging_event_h

#include <mach/mach.h>
#include "logger_internal.h"

typedef enum { EventType_Invalid = 0, EventType_Alloc, EventType_Free, EventType_Update, EventType_Stack } event_type;

typedef struct {
    uint64_t address;
    uint32_t size;
    uint32_t object_type;
    uint32_t type_flags;
    uint16_t event_size;
    uint8_t event_type;
    uint8_t stack_size;
    uint64_t stack_hash;
    uintptr_t stacks[STACK_LOGGING_MAX_STACK_SIZE];
} memory_logging_event;

#define MEMORY_LOGGING_EVENT_SIMPLE_SIZE offsetof(memory_logging_event, stack_hash)

inline size_t alloc_event_size(memory_logging_event *event) {
    return offsetof(memory_logging_event, stacks) + (event->stack_size << 3); // event->stack_size * sizeof(uintptr_t);
}

#endif /* memory_logging_event_h */
