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

#include <stdlib.h>
#include <unistd.h>
#include <mach/mach.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <assert.h>
#include "stack_frames_db.h"
#include "logger_internal.h"
#include "splay_tree.h"

#pragma mark -
#pragma mark Defines

// The expansion factor controls the shifting up of table size. A factor of 1 will double the size upon expanding,
// 2 will quadruple the size, etc. Maintaining a 66% fill in an ideal table requires the collision allowance to
// increase by 3 for every quadrupling of the table size (although this the constant applied to insertion
// performance O(c*n))
#define EXPAND_FACTOR 2
#define COLLISION_GROWTH_RATE 3

// For a uniquing table, the useful node size is slots := floor(table_byte_size / sizeof(table_slot_t))
// Some useful numbers for the initial max collision value (desiring 66% fill):
#define INITIAL_MAX_COLLIDE	23
#define DEFAULT_UNIQUING_BAND_SIZE (8 << 20) // 8M

#define STACK_FRAMES_DB_DIR			"db"
#define STACK_FRAMES_DB_FILE		"stack_frames.db"
#define STACK_FRAMES_DB_INDEX_FILE	"stack_frames.db.index"

#pragma mark -
#pragma mark Types

struct backtrace_uniquing_table {
	int			fd;
	uint64_t	fs;
	uint64_t	num_nodes;
	uint64_t	hash_multiplier;
	uint64_t	band_count;
	uint64_t	nodes_full;
	uint64_t	frames_count;
	uint64_t	header_bytes;
	
	uint64_t	*last_band;
	
	// for search
	uint64_t	band_index;
	uint64_t	*curr_band;
};

typedef uint64_t slot_address;
typedef uint64_t slot_parent;

#pragma pack(push,8)
struct table_slot_t {
	union {
		uint64_t			value;
		
		struct {
			slot_address	address:36;
			slot_parent		uParent:28;
		} detail;
	};
};
#pragma pack(pop)

struct stack_entry_t {
	uint32_t stack_hash;
	uint32_t stack_identifier;
	
	stack_entry_t(uint32_t _s=0, uint32_t _e=0) : stack_hash(_s), stack_identifier(_e) {}
	
	inline bool operator != (const stack_entry_t &another) const {
		return stack_hash != another.stack_hash;
	}
	
	inline bool operator == (const stack_entry_t &another) const {
		return stack_hash == another.stack_hash;
	}
	
	inline bool operator > (const stack_entry_t &another) const {
		return stack_hash > another.stack_hash;
	}
	
	inline bool operator < (const stack_entry_t &another) const {
		return stack_hash < another.stack_hash;
	}
};

struct backtrace_uniquing_table_index {
	int		fd;
	size_t	fs;
	splay_tree<stack_entry_t> *stack_entry_list;
};

struct stack_frames_db {
	backtrace_uniquing_table		*table;
	backtrace_uniquing_table_index	*table_index;
};

#pragma mark -
#pragma mark In-Memory Backtrace Uniquing

// pre-declarations
static bool __expand_uniquing_table(backtrace_uniquing_table *);

static backtrace_uniquing_table *__init_uniquing_table(const char *dir_path)
{
	static uint64_t header_bytes = round_page(sizeof(backtrace_uniquing_table));
	
	int fd = -1;
	backtrace_uniquing_table *uniquing_table = NULL;
	
	// this call ensures that the log files exist
	fd = open_file(dir_path, STACK_FRAMES_DB_FILE);
	if (fd < 0) {
		err_code = MS_ERRC_SF_TABLE_FILE_OPEN_FAIL;
		goto init_fail;
	} else {
		struct stat st = {0};
		if (fstat(fd, &st) == -1) {
			err_code = MS_ERRC_SF_TABLE_FILE_SIZE_FAIL;
			goto init_fail;
		} else {
			if (st.st_size > header_bytes && (st.st_size - header_bytes) % DEFAULT_UNIQUING_BAND_SIZE == 0) {
				uniquing_table = (backtrace_uniquing_table *)inter_mmap(NULL, (size_t)header_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
				if (uniquing_table == MAP_FAILED) {
					__malloc_printf("fail to mmap, %s", strerror(errno));
					err_code = MS_ERRC_SF_TABLE_FILE_MMAP_FAIL;
					goto init_fail;
				} else {
					uniquing_table->fd = fd;
					uniquing_table->fs = st.st_size;
					uniquing_table->last_band = (uint64_t *)inter_mmap(NULL, DEFAULT_UNIQUING_BAND_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, st.st_size - DEFAULT_UNIQUING_BAND_SIZE);
					
					uniquing_table->band_index = 0;
					uniquing_table->curr_band = NULL;
				}
			} else {
				if (ftruncate(fd, header_bytes) != 0) {
					__malloc_printf("ftruncate fail, %s", strerror(errno));
					err_code = MS_ERRC_SF_TABLE_FILE_TRUNCATE_FAIL;
					goto init_fail;
				}
				uniquing_table = (backtrace_uniquing_table *)inter_mmap(NULL, (size_t)header_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
				if (uniquing_table == MAP_FAILED) {
					__malloc_printf("fail to mmap, %s", strerror(errno));
					err_code = MS_ERRC_SF_TABLE_FILE_MMAP_FAIL;
					goto init_fail;
				} else {
					bzero(uniquing_table, sizeof(backtrace_uniquing_table));
					uniquing_table->num_nodes = DEFAULT_UNIQUING_BAND_SIZE / sizeof(table_slot_t) - 1;
					uniquing_table->hash_multiplier = (int32_t)uniquing_table->num_nodes / (INITIAL_MAX_COLLIDE * 2 + 1);
					
					uniquing_table->fd = fd;
					uniquing_table->fs = header_bytes;
					uniquing_table->header_bytes = header_bytes;
					__expand_uniquing_table(uniquing_table);
					
					uniquing_table->band_index = 0;
					uniquing_table->curr_band = NULL;
				}
			}
		}
	}
	return uniquing_table;
	
init_fail:
	if (fd >= 0) close(fd);
	return NULL;
}

static void __destory_uniquing_table(backtrace_uniquing_table *uniquing_table)
{
	if (uniquing_table == NULL) {
		return;
	}

	static uint64_t header_bytes = round_page(sizeof(backtrace_uniquing_table));
		
	int fd = uniquing_table->fd;
	if (uniquing_table->last_band) inter_munmap(uniquing_table->last_band, DEFAULT_UNIQUING_BAND_SIZE);
	if (uniquing_table->curr_band) inter_munmap(uniquing_table->curr_band, DEFAULT_UNIQUING_BAND_SIZE);
	inter_munmap(uniquing_table, (size_t)header_bytes);
	close(fd);
}

static bool __expand_uniquing_table(backtrace_uniquing_table *uniquing_table)
{
	if (ftruncate(uniquing_table->fd, uniquing_table->fs + DEFAULT_UNIQUING_BAND_SIZE) != 0) {
		__malloc_printf("fail to ftruncate, %s, new_size: %llu, errno: %d", strerror(errno), (uint64_t)uniquing_table->fs + DEFAULT_UNIQUING_BAND_SIZE, errno);
		disable_memory_logging();
		abort();
		return false;
	}
	
	uint64_t *newBand = (uint64_t *)inter_mmap(NULL, DEFAULT_UNIQUING_BAND_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, uniquing_table->fd, uniquing_table->fs);
	if (newBand == MAP_FAILED) {
		__malloc_printf("fail to mmap, %s, new_size: %llu, errno: %d", strerror(errno), (uint64_t)DEFAULT_UNIQUING_BAND_SIZE, errno);
//		disable_memory_logging();
//		abort();
		return false;
	}
	
	if (uniquing_table->last_band) {
		inter_munmap(uniquing_table->last_band, DEFAULT_UNIQUING_BAND_SIZE);
	}
	
	uniquing_table->last_band = newBand;
	uniquing_table->band_count++;
	uniquing_table->fs += DEFAULT_UNIQUING_BAND_SIZE; // refresh file size
	__malloc_printf("uniquing table new file size: %llu", uniquing_table->fs);
	
	return true;
}

// The hash values need to be the same size as the addresses, for clarity, define a new type
typedef vm_address_t hash_index_t;

hash_index_t __enter_address_in_band(backtrace_uniquing_table *uniquing_table, uint64_t address, hash_index_t uParent)
{
	int32_t collisions = INITIAL_MAX_COLLIDE;
	
	if (address >= (1ll << 36)) {
		address = ((1ll << 36) - 1);
	}
	
	hash_index_t hash = (hash_index_t)(((uParent << 4) ^ (address >> 2)) % uniquing_table->num_nodes + 1);
    hash_index_t hash_multiplier = (hash_index_t)(uniquing_table->hash_multiplier);
	table_slot_t new_slot = {.detail.address=address, .detail.uParent=uParent};
	
	while (collisions--) {
		table_slot_t *table_slot = (table_slot_t *)uniquing_table->last_band + hash;
		uint64_t t_value = table_slot->value;
		
		if (t_value == 0) {
			// blank; store this entry!
			table_slot->value = new_slot.value;
			uniquing_table->nodes_full++;
			return hash;
		}
		if (t_value == new_slot.value) {
			// hit! retrieve index and go.
			return hash;
		}
		
		hash += collisions * hash_multiplier + 1;
		
		if (hash > uniquing_table->num_nodes) {
			hash -= uniquing_table->num_nodes; // wrap around.
		}
	}
	
	return 0; // can not find any empty slot
}

uint32_t __enter_frames_in_table(backtrace_uniquing_table *uniquing_table, uintptr_t *frames, int32_t count)
{
	int32_t lcopy = count;
	hash_index_t uParent = 0;
	hash_index_t baseIndex = (hash_index_t)((uniquing_table->band_count - 1) * uniquing_table->num_nodes);
	
	while (--lcopy >= 0) {
		uint64_t thisPC = frames[lcopy];
		if (thisPC == 0) continue;
		
		while (true) {
			hash_index_t bandIndex = __enter_address_in_band(uniquing_table, thisPC, uParent);
			if (bandIndex >= (1 << 28)) {
				abort();
			}
			if (bandIndex != 0) {
				uParent = bandIndex + baseIndex;
				break;
			} else {
				if (__expand_uniquing_table(uniquing_table) == false) {
					return 0; // expand fail
				} else {
					baseIndex += uniquing_table->num_nodes;
				}
			}
		}
	}
	
	uniquing_table->frames_count += count;
	
	return (uint32_t)uParent;
}

table_slot_t *__get_node_from_table(backtrace_uniquing_table *uniquing_table, uint32_t index_pos)
{
	static uint64_t header_bytes = round_page(sizeof(backtrace_uniquing_table));
	
	uint64_t band_index = (index_pos - 1) / uniquing_table->num_nodes;
	if ((band_index + 1) * DEFAULT_UNIQUING_BAND_SIZE + header_bytes > uniquing_table->fs) {
		__malloc_printf("get node fail, band_index: %llu, fs: %llu, index_pos: %u, num_nodes: %llu", band_index, uniquing_table->fs, index_pos, uniquing_table->num_nodes);
		return NULL;
	}
	if (band_index != uniquing_table->band_index || uniquing_table->curr_band == NULL) {
		if (uniquing_table->curr_band) {
			inter_munmap(uniquing_table->curr_band, DEFAULT_UNIQUING_BAND_SIZE);
			uniquing_table->curr_band = NULL;
		}
		uint64_t *curr_band = (uint64_t *)inter_mmap(NULL, DEFAULT_UNIQUING_BAND_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, uniquing_table->fd, header_bytes + band_index * DEFAULT_UNIQUING_BAND_SIZE);
		if (curr_band == MAP_FAILED) {
			return NULL;
		}
		uniquing_table->curr_band = curr_band;
		uniquing_table->band_index = band_index;
	}
	
	index_pos -= band_index * uniquing_table->num_nodes;
	return (table_slot_t *)uniquing_table->curr_band + index_pos;
}

#pragma mark -
#pragma mark Table Index

static void *__uniquing_table_index_reallocator(void *context, void *old_mem, size_t new_size)
{
	backtrace_uniquing_table_index *table_index = (backtrace_uniquing_table_index *)context;
	
	if (old_mem) {
		inter_munmap(old_mem, table_index->fs);
	}
	
	new_size = round_page(new_size);
	if (ftruncate(table_index->fd, new_size) != 0) {
		__malloc_printf("fail to ftruncate, %s, new_size: %llu, errno: %d", strerror(errno), (uint64_t)new_size, errno);
		disable_memory_logging();
		abort();
		return NULL;
	}
	
	void *new_mem = inter_mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, table_index->fd, 0);
	if (new_mem == MAP_FAILED) {
		__malloc_printf("fail to mmap, %s, new_size: %llu, errno: %d", strerror(errno), (uint64_t)new_size, errno);
//		disable_memory_logging();
//		abort();
		return NULL;
	}
	
	table_index->fs = new_size;
	__malloc_printf("uniquing table index new file size: %lu", table_index->fs);
	
	return new_mem;
}

static void __uniquing_table_index_deallocator(void *context, void *mem)
{
	backtrace_uniquing_table_index *table_index = (backtrace_uniquing_table_index *)context;
	
	if (mem) {
		inter_munmap(mem, table_index->fs);
	}
}

static backtrace_uniquing_table_index *__init_uniquing_table_index(const char *dir_path)
{
	int fd = open_file(dir_path, STACK_FRAMES_DB_INDEX_FILE);
	backtrace_uniquing_table_index *table_index = (backtrace_uniquing_table_index *)inter_malloc(sizeof(backtrace_uniquing_table_index));
	
	if (fd < 0) {
		// should not be happened
		err_code = MS_ERRC_SF_INDEX_FILE_OPEN_FAIL;
		goto init_fail;
	} else {
		struct stat st = {0};
		if (fstat(fd, &st) == -1) {
			err_code = MS_ERRC_SF_INDEX_FILE_SIZE_FAIL;
			goto init_fail;
		} else {
			table_index->fd = fd;
			table_index->fs = (size_t)st.st_size;
			
			if (st.st_size > 0) {
				void *buff = inter_mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
				if (buff == MAP_FAILED) {
					__malloc_printf("fail to mmap, %s", strerror(errno));
					err_code = MS_ERRC_SF_INDEX_FILE_MMAP_FAIL;
					goto init_fail;
				} else {
					table_index->stack_entry_list = new splay_tree<stack_entry_t>(1 << 18, __uniquing_table_index_reallocator, __uniquing_table_index_deallocator, buff, (size_t)st.st_size, table_index);
				}
			} else {
				table_index->stack_entry_list = new splay_tree<stack_entry_t>(1 << 18, __uniquing_table_index_reallocator, __uniquing_table_index_deallocator, NULL, 0, table_index);
			}
		}
	}
	return table_index;
	
init_fail:
	if (fd >= 0) close(fd);
	inter_free(table_index);
	return NULL;
}

static void __destory_uniquing_table_index(backtrace_uniquing_table_index *table_index)
{
	if (table_index == NULL) {
		return;
	}
	
	delete table_index->stack_entry_list;
	close(table_index->fd);
	inter_free(table_index);
}

__attribute__((always_inline))
static bool __is_stack_hash_exist(backtrace_uniquing_table_index *table_index, uint32_t stack_hash)
{
	return table_index->stack_entry_list->exist(stack_hash);
}

__attribute__((always_inline))
static void __add_stack_entry(backtrace_uniquing_table_index *table_index, uint32_t stack_hash, uint32_t entry_identifier)
{
	table_index->stack_entry_list->insert(stack_entry_t(stack_hash, entry_identifier));
}

__attribute__((always_inline))
static uint32_t __stack_identifier_for_hash(backtrace_uniquing_table_index *table_index, uint32_t stack_hash)
{
	return table_index->stack_entry_list->find().stack_identifier;
}

#pragma mark -
#pragma mark Public Interface

static uint32_t __frames_hash(uintptr_t *frames, int32_t count)
{
	uint64_t hash = 0;
	
	for (int i = 0; i < count; ++i, ++frames) {
		hash = (hash << 8) ^ (hash >> 56) ^ *frames;
	}
	
	// 6 bits for count(<=64)
	return (uint32_t)((hash << 6) | count);
}

stack_frames_db *open_or_create_stack_frames_db(const char *db_dir)
{
	backtrace_uniquing_table		*table = __init_uniquing_table(db_dir);
	backtrace_uniquing_table_index	*table_index = __init_uniquing_table_index(db_dir);
	if (table == NULL || table_index == NULL) {
		__malloc_printf("init uniquing table or index fail, %p, %p", table, table_index);
		return NULL;
	}
	
	stack_frames_db *db_context = (stack_frames_db *)inter_malloc(sizeof(stack_frames_db));
	db_context->table = table;
	db_context->table_index = table_index;
	return db_context;
}

void close_stack_frames_db(stack_frames_db *db_context)
{
	if (db_context == NULL) {
		return;
	}
	
	__destory_uniquing_table(db_context->table);
	__destory_uniquing_table_index(db_context->table_index);
}

uint32_t add_stack_frames_in_table(stack_frames_db *db_context, uintptr_t *frames, int32_t count)
{
	uint32_t stack_hash = __frames_hash(frames, count);
	uint32_t stack_identifier = 0;
	
	// Check whether this stack exists
	if (!__is_stack_hash_exist(db_context->table_index, stack_hash)) {
		stack_identifier = __enter_frames_in_table(db_context->table, frames, count);
		__add_stack_entry(db_context->table_index, stack_hash, stack_identifier);
	} else {
		stack_identifier = __stack_identifier_for_hash(db_context->table_index, stack_hash);
	}
	
	if (!stack_identifier) abort();
	return stack_identifier;
}

void unwind_stack_from_table_index(stack_frames_db *db_context, uint32_t stack_identifier, uint64_t *out_frames_buffer, uint32_t *out_frames_count, uint32_t max_frames)
{
	table_slot_t *node = __get_node_from_table(db_context->table, stack_identifier);
	uint32_t foundFrames = 0;
	while (node && foundFrames < max_frames) {
		out_frames_buffer[foundFrames++] = node->detail.address;
		if (node->detail.uParent == 0) break;
		node = __get_node_from_table(db_context->table, node->detail.uParent);
		assert(foundFrames < max_frames || node == NULL);
	}

	*out_frames_count = foundFrames;
}
