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

#ifndef allocation_event_buffer_h
#define allocation_event_buffer_h

#include <mach/mach.h>
#include "logger_internal.h"

#define STACK_LOGGING_MAX_STACK_SIZE		(64)
#define STACK_LOGGING_EVENT_BUFFER_SIZE		(4ll << 20) // Must be 2^n!!

typedef enum {
	EventType_Invalid = 0,
	EventType_Alloc,
	EventType_Free,
	EventType_Update
} event_type;

typedef struct {
	uint64_t	address;
	uint32_t	argument; // for allocation event, argument is malloc size; for update event, argument is object type
	uint32_t	t_id; // for dota...
	uint32_t	type_flags;
	uint8_t		event_type;
	uint8_t		stack_size;
	uint8_t		num_hot_to_skip;
	uintptr_t	stacks[STACK_LOGGING_MAX_STACK_SIZE];
} memory_logging_event;

struct allocation_event_buffer;

allocation_event_buffer *open_or_create_allocation_event_buffer(const char *event_dir);
// Do not use file storage
allocation_event_buffer *open_or_create_allocation_event_buffer_static();
void close_allocation_event_buffer(allocation_event_buffer *event_buff);

void append_event_to_buffer(allocation_event_buffer *event_buff, memory_logging_event *new_event);
bool has_event_in_buffer(allocation_event_buffer *event_buff, int64_t from_index = -1);
memory_logging_event *get_event_from_buffer(allocation_event_buffer *event_buff, int64_t *next_index, int64_t from_index = -1);

bool is_next_index_valid(allocation_event_buffer *event_buff, int64_t next_index);
void update_read_index(allocation_event_buffer *event_buff, int64_t next_index);
void reset_write_index(allocation_event_buffer *event_buff);

#endif /* allocation_event_buffer_h */
