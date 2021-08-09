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

#ifndef memory_logging_event_buffer_list_h
#define memory_logging_event_buffer_list_h

#include "memory_logging_event_buffer.h"

struct memory_logging_event_buffer_list;

memory_logging_event_buffer_list *memory_logging_event_buffer_list_create();
void memory_logging_event_buffer_list_free(memory_logging_event_buffer_list *buffer_list);

void memory_logging_event_buffer_list_push_back(memory_logging_event_buffer_list *buffer_list, memory_logging_event_buffer *event_buffer);
memory_logging_event_buffer *memory_logging_event_buffer_list_pop_all(memory_logging_event_buffer_list *buffer_list);

#endif /* memory_logging_event_buffer_list_h */
