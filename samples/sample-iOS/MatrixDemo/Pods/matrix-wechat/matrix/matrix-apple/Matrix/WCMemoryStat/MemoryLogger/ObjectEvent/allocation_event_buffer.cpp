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

#include "allocation_event_buffer.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#pragma mark -
#pragma mark Types

struct allocation_event_buffer {
	int fd;
	int fs;
	malloc_lock_s lock;
	
	volatile int64_t read_index;
	volatile int64_t write_index;
	uint8_t buffer[STACK_LOGGING_EVENT_BUFFER_SIZE + 1024]; // make sure sizeof(memory_logging_event) is smaller than 1024 bytes
};

#pragma mark -
#pragma mark Constants/Globals

static int simple_size = offsetof(memory_logging_event, stacks);

#pragma mark -
#pragma mark Functions

allocation_event_buffer *open_or_create_allocation_event_buffer(const char *event_dir)
{
	int fd = open_file(event_dir, "file_index.dat");
	int fs = (int)round_page(sizeof(allocation_event_buffer));
	allocation_event_buffer *event_buffer = NULL;
	
	if (fd < 0) {
		err_code = MS_ERRC_DI_FILE_OPEN_FAIL;
		goto init_fail;
	} else {
		struct stat st = {0};
		if (fstat(fd, &st) == -1) {
			err_code = MS_ERRC_DI_FILE_SIZE_FAIL;
			goto init_fail;
		}
		if (st.st_size == 0 || st.st_size != fs) {
			// new file
			if (ftruncate(fd, fs) != 0) {
				err_code = MS_ERRC_DI_FILE_TRUNCATE_FAIL;
				goto init_fail;
			}
			
			void *buff = inter_mmap(NULL, fs, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (buff == MAP_FAILED) {
				err_code = MS_ERRC_DI_FILE_MMAP_FAIL;
				goto init_fail;
			}
			
			event_buffer = (allocation_event_buffer *)buff;
			event_buffer->read_index = 0;
			event_buffer->write_index = 0;
		} else {
			void *buff = inter_mmap(NULL, fs, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (buff == MAP_FAILED) {
				err_code = MS_ERRC_DI_FILE_MMAP_FAIL;
				goto init_fail;
			}
			
			event_buffer = (allocation_event_buffer *)buff;
		}
	}
	
	event_buffer->fd = fd;
	event_buffer->fs = fs;
	event_buffer->lock = __malloc_lock_init();

	return event_buffer;
	
init_fail:
	if (fd >= 0) close(fd);
	return NULL;
}

allocation_event_buffer *open_or_create_allocation_event_buffer_static()
{
	allocation_event_buffer *event_buffer = (allocation_event_buffer *)inter_malloc(sizeof(allocation_event_buffer));
	
	event_buffer->fd = -1;
	event_buffer->fs = 0;
	event_buffer->read_index = 0;
	event_buffer->write_index = 0;
	event_buffer->lock = __malloc_lock_init();
	
	return event_buffer;
}

void close_allocation_event_buffer(allocation_event_buffer *event_buff)
{
	if (event_buff == NULL) {
		return;
	}
	
	int fd = event_buff->fd;
	if (fd >= 0) {
		inter_munmap(event_buff, event_buff->fs);
		close(fd);
	} else {
		inter_free(event_buff);
	}
}

void append_event_to_buffer(allocation_event_buffer *event_buff, memory_logging_event *new_event)
{
	int write_size = simple_size + new_event->stack_size * sizeof(uintptr_t);
	
	__malloc_lock_lock(&event_buff->lock);
	
	while (event_buff->write_index - event_buff->read_index > (STACK_LOGGING_EVENT_BUFFER_SIZE - sizeof(memory_logging_event)) - write_size) {
		usleep(2000);
	}
	
	int64_t start_index = (event_buff->write_index & (STACK_LOGGING_EVENT_BUFFER_SIZE - 1)); // must be 2^n
	memcpy(event_buff->buffer + start_index, new_event, write_size);
	int64_t new_write_index = event_buff->write_index + write_size;
	event_buff->write_index = new_write_index;
	// 32-bit machine is not compatible
	//OSAtomicAdd64(write_size, &event_buff->write_index);
	
	__malloc_lock_unlock(&event_buff->lock);
}

__attribute__((always_inline))
bool has_event_in_buffer(allocation_event_buffer *event_buff, int64_t from_index)
{
	if (from_index < 0) from_index = event_buff->read_index;
	return from_index < event_buff->write_index;
}

memory_logging_event *get_event_from_buffer(allocation_event_buffer *event_buff, int64_t *next_index, int64_t from_index)
{
	if (from_index < 0) from_index = event_buff->read_index;
	int64_t start_index = (from_index & (STACK_LOGGING_EVENT_BUFFER_SIZE - 1)); // must be 2^n
	memory_logging_event *next_event = (memory_logging_event *)(event_buff->buffer + start_index);
	if (next_index) *next_index = from_index + simple_size + next_event->stack_size * sizeof(uintptr_t);
	
	return next_event;
}

__attribute__((always_inline))
bool is_next_index_valid(allocation_event_buffer *event_buff, int64_t next_index)
{
	return next_index <= event_buff->write_index;
}

__attribute__((always_inline))
void update_read_index(allocation_event_buffer *event_buff, int64_t next_index)
{
	event_buff->read_index = next_index;
	// 32-bit machine is not compatible
	//OSAtomicCompareAndSwap64(event_buff->read_index, next_index, &event_buff->read_index);
}

__attribute__((always_inline))
void reset_write_index(allocation_event_buffer *event_buff)
{
	event_buff->read_index = 0;
	event_buff->write_index = 0;
}
