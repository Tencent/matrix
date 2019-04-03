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

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "allocation_event_db.h"
#include "logger_internal.h"
#include "splay_tree.h"

#pragma mark -
#pragma mark Types

struct allocation_event_db {
	int		fd;
	size_t	fs;
	splay_tree<allocation_event> *allocation_event_list;
};

#pragma mark -
#pragma mark Public Interface

static void *__allocation_events_reallocator(void *context, void *old_mem, size_t new_size)
{
	allocation_event_db *db_context = (allocation_event_db *)context;
	
	if (old_mem) {
		inter_munmap(old_mem, db_context->fs);
	}
	
	new_size = round_page(new_size);
	if (ftruncate(db_context->fd, new_size) != 0) {
		__malloc_printf("fail to ftruncate, %s, new_size: %llu, errno: %d", strerror(errno), (uint64_t)new_size, errno);
		disable_memory_logging();
		abort();
		return NULL;
	}
	
	void *new_mem = inter_mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, db_context->fd, 0);
	if (new_mem == MAP_FAILED) {
		__malloc_printf("fail to mmap, %s, new_size: %llu, errno: %d", strerror(errno), (uint64_t)new_size, errno);
//		disable_memory_logging();
//		abort();
		return NULL;
	}
	
	db_context->fs = new_size;
	__malloc_printf("allocation event new file size: %lu", db_context->fs);
	
	return new_mem;
}

static void __allocation_events_deallocator(void *context, void *mem)
{
	allocation_event_db *db_context = (allocation_event_db *)context;
	
	if (mem) {
		inter_munmap(mem, db_context->fs);
	}
}

allocation_event_db *open_or_create_allocation_event_db(const char *event_dir)
{
	int fd = -1;
	allocation_event_db *db_context = (allocation_event_db *)inter_malloc(sizeof(allocation_event_db));
	
	fd = open_file(event_dir, "allocation_events.dat");
	if (fd < 0) {
		// should not be happened
		err_code = MS_ERRC_AE_FILE_OPEN_FAIL;
		goto init_fail;
	} else {
		struct stat st = {0};
		if (fstat(fd, &st) == -1) {
			err_code = MS_ERRC_AE_FILE_SIZE_FAIL;
			goto init_fail;
		} else {
			db_context->fd = fd;
			db_context->fs = (size_t)st.st_size;
			
			if (st.st_size > 0) {
				void *buff = inter_mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
				if (buff == MAP_FAILED) {
					__malloc_printf("fail to mmap, %s", strerror(errno));
					err_code = MS_ERRC_AE_FILE_MMAP_FAIL;
					goto init_fail;
				} else {
					db_context->allocation_event_list = new splay_tree<allocation_event>(102400, __allocation_events_reallocator, __allocation_events_deallocator, buff, (size_t)st.st_size, db_context);
				}
			} else {
				db_context->allocation_event_list = new splay_tree<allocation_event>(102400, __allocation_events_reallocator, __allocation_events_deallocator, NULL, 0, db_context);
			}
		}
	}
	return db_context;
		
init_fail:
	if (fd >= 0) close(fd);
	inter_free(db_context);
	return NULL;
}

void close_allocation_event_db(allocation_event_db *db_context)
{
	if (db_context == NULL) {
		return;
	}
	
	delete db_context->allocation_event_list;
	close(db_context->fd);
	inter_free(db_context);
}

void add_allocation_event(allocation_event_db *db_context, uint64_t address, uint32_t type_flags, uint32_t object_type, uint32_t size, uint32_t stack_identifier, uint32_t t_id)
{
	db_context->allocation_event_list->insert(allocation_event(address, type_flags & memory_logging_valid_type_flags, object_type, stack_identifier, size, t_id));
}

void del_allocation_event(allocation_event_db *db_context, uint64_t address, uint32_t type_flags)
{
	db_context->allocation_event_list->remove(address);
}

void update_allocation_event_object_type(allocation_event_db *db_context, uint64_t address, uint32_t new_type)
{
	// check if exist
	if (db_context->allocation_event_list->exist(address)) {
		db_context->allocation_event_list->find().object_type = new_type;
	}
}

void enumerate_allocation_event(allocation_event_db *db_context, void (^callback)(const allocation_event &event))
{
	db_context->allocation_event_list->enumerate(callback);
}
