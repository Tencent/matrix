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
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <assert.h>

#include "stack_frames_db.h"
#include "splay_map_ptr.h"

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
#define INITIAL_MAX_COLLIDE 23
#define DEFAULT_UNIQUING_BAND_SIZE (8 << 20) // 8M

#define STACK_FRAMES_MAX_LEVEL STACK_LOGGING_MAX_STACK_SIZE

#define STACK_FRAMES_DB_DIR "db"
#define STACK_FRAMES_DB_FILE "stack_frames.db"
#define STACK_FRAMES_DB_INDEX_FILE "stack_frames.db.index"

#pragma mark -
#pragma mark Types

struct backtrace_uniquing_table {
    int fd;
    uint64_t fs;
    uint64_t num_nodes;
    uint64_t hash_multiplier;
    uint64_t band_count;
    uint64_t nodes_full;
    uint64_t frames_count;
    uint64_t header_bytes;

    uint64_t *last_band;

    // for search
    uint64_t band_index;
    uint64_t *curr_band;
};

typedef uint64_t slot_address;
typedef uint64_t slot_parent;

#pragma pack(push, 8)
struct table_slot_t {
    union {
        uint64_t value;

        struct {
            slot_address address : 36;
            slot_parent uParent : 28;
        } detail;
    };
};
#pragma pack(pop)

typedef splay_map_ptr<uint64_t, uint32_t> stack_entry_list_t;

struct backtrace_uniquing_table_index {
    memory_pool_file *memory_pool;
    stack_entry_list_t *stack_entry_list[STACK_FRAMES_MAX_LEVEL]; // stack_hash -> entry_identifier
};

struct stack_frames_db {
    backtrace_uniquing_table *table;
    backtrace_uniquing_table_index *table_index;
};

#pragma mark -
#pragma mark In-Memory Backtrace Uniquing

// pre-declarations
static bool __expand_uniquing_table(backtrace_uniquing_table *);

static backtrace_uniquing_table *__init_uniquing_table(const char *dir_path) {
    static uint64_t header_bytes = round_page(sizeof(backtrace_uniquing_table));

    int fd = -1;
    backtrace_uniquing_table *uniquing_table = NULL;

    // this call ensures that the log files exist
    fd = open_file(dir_path, STACK_FRAMES_DB_FILE);
    if (fd < 0) {
        err_code = MS_ERRC_SF_TABLE_FILE_OPEN_FAIL;
        goto init_fail;
    } else {
        struct stat st = { 0 };
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
                    uniquing_table->last_band = (uint64_t *)
                    inter_mmap(NULL, DEFAULT_UNIQUING_BAND_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, st.st_size - DEFAULT_UNIQUING_BAND_SIZE);

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
    if (fd >= 0)
        close(fd);
    return NULL;
}

static void __destory_uniquing_table(backtrace_uniquing_table *uniquing_table) {
    if (uniquing_table == NULL) {
        return;
    }

    static uint64_t header_bytes = round_page(sizeof(backtrace_uniquing_table));

    int fd = uniquing_table->fd;
    if (uniquing_table->last_band)
        inter_munmap(uniquing_table->last_band, DEFAULT_UNIQUING_BAND_SIZE);
    if (uniquing_table->curr_band)
        inter_munmap(uniquing_table->curr_band, DEFAULT_UNIQUING_BAND_SIZE);
    inter_munmap(uniquing_table, (size_t)header_bytes);
    close(fd);
}

static bool __expand_uniquing_table(backtrace_uniquing_table *uniquing_table) {
    if (ftruncate(uniquing_table->fd, uniquing_table->fs + DEFAULT_UNIQUING_BAND_SIZE) != 0) {
        disable_memory_logging();
        __malloc_printf("fail to ftruncate, %s, new_size: %llu, errno: %d",
                        strerror(errno),
                        (uint64_t)uniquing_table->fs + DEFAULT_UNIQUING_BAND_SIZE,
                        errno);
        abort();
        return false;
    }

    uint64_t *newBand =
    (uint64_t *)inter_mmap(NULL, DEFAULT_UNIQUING_BAND_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, uniquing_table->fd, uniquing_table->fs);
    if (newBand == MAP_FAILED) {
        disable_memory_logging();
        __malloc_printf("fail to mmap, %s, new_size: %llu, errno: %d", strerror(errno), (uint64_t)DEFAULT_UNIQUING_BAND_SIZE, errno);
        abort();
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

hash_index_t __enter_address_in_band(backtrace_uniquing_table *uniquing_table, uint64_t address, hash_index_t uParent) {
    int32_t collisions = INITIAL_MAX_COLLIDE;

    if (address >= (1ll << 36)) {
        address = ((1ll << 36) - 1);
    }

    hash_index_t hash = (hash_index_t)(((uParent << 4) ^ (address >> 2)) % uniquing_table->num_nodes + 1);
    hash_index_t hash_multiplier = (hash_index_t)(uniquing_table->hash_multiplier);
    table_slot_t new_slot = { .detail.address = address, .detail.uParent = uParent };

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

uint32_t __enter_frames_in_table(backtrace_uniquing_table *uniquing_table, uintptr_t *frames, int32_t count) {
    int32_t lcopy = count;
    hash_index_t uParent = 0;
    hash_index_t baseIndex = (hash_index_t)((uniquing_table->band_count - 1) * uniquing_table->num_nodes);

    while (--lcopy >= 0) {
        uint64_t thisPC = frames[lcopy];
        if (thisPC == 0)
            continue;

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

table_slot_t *__get_node_from_table(backtrace_uniquing_table *uniquing_table, uint32_t index_pos) {
    static uint64_t header_bytes = round_page(sizeof(backtrace_uniquing_table));

    uint64_t band_index = (index_pos - 1) / uniquing_table->num_nodes;
    if ((band_index + 1) * DEFAULT_UNIQUING_BAND_SIZE + header_bytes > uniquing_table->fs) {
        __malloc_printf("get node fail, band_index: %llu, fs: %llu, index_pos: %u, num_nodes: %llu",
                        band_index,
                        uniquing_table->fs,
                        index_pos,
                        uniquing_table->num_nodes);
        return NULL;
    }
    if (band_index != uniquing_table->band_index || uniquing_table->curr_band == NULL) {
        if (uniquing_table->curr_band) {
            inter_munmap(uniquing_table->curr_band, DEFAULT_UNIQUING_BAND_SIZE);
            uniquing_table->curr_band = NULL;
        }
        uint64_t *curr_band = (uint64_t *)inter_mmap(NULL,
                                                     DEFAULT_UNIQUING_BAND_SIZE,
                                                     PROT_READ | PROT_WRITE,
                                                     MAP_SHARED,
                                                     uniquing_table->fd,
                                                     header_bytes + band_index * DEFAULT_UNIQUING_BAND_SIZE);
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

static void __destory_uniquing_table_index(backtrace_uniquing_table_index *table_index) {
    if (table_index == NULL) {
        return;
    }

    for (int i = 0; i < STACK_FRAMES_MAX_LEVEL; ++i) {
        delete table_index->stack_entry_list[i];
    }
    delete table_index->memory_pool;
    inter_free(table_index);
}

static backtrace_uniquing_table_index *__init_uniquing_table_index(const char *dir_path) {
    backtrace_uniquing_table_index *table_index = (backtrace_uniquing_table_index *)inter_malloc(sizeof(backtrace_uniquing_table_index));

    table_index->memory_pool = new memory_pool_file(dir_path, STACK_FRAMES_DB_INDEX_FILE);
    if (table_index->memory_pool->init_fail()) {
        goto init_fail;
    }

    for (int i = 0; i < STACK_FRAMES_MAX_LEVEL; ++i) {
        table_index->stack_entry_list[i] = new stack_entry_list_t(table_index->memory_pool);
    }

    return table_index;

init_fail:
    __destory_uniquing_table_index(table_index);
    return NULL;
}

#pragma mark -
#pragma mark Public Interface

static uint64_t __frames_hash(uintptr_t *frames, int32_t count) {
    uint64_t hash = 0;
    uintptr_t *end = frames + count;
    size_t seed = 131; // 31 131 1313 13131 131313 etc..

    while (frames < end) {
        hash = hash * seed + (*frames++);
    }

    return hash;
}

stack_frames_db *stack_frames_db_open_or_create(const char *db_dir) {
    backtrace_uniquing_table *table = __init_uniquing_table(db_dir);
    backtrace_uniquing_table_index *table_index = __init_uniquing_table_index(db_dir);
    if (table == NULL || table_index == NULL) {
        __malloc_printf("init uniquing table or index fail, %p, %p", table, table_index);
        return NULL;
    }

    stack_frames_db *db_context = (stack_frames_db *)inter_malloc(sizeof(stack_frames_db));
    db_context->table = table;
    db_context->table_index = table_index;
    return db_context;
}

void stack_frames_db_close(stack_frames_db *db_context) {
    if (db_context == NULL) {
        return;
    }

    __destory_uniquing_table(db_context->table);
    __destory_uniquing_table_index(db_context->table_index);
}

uint32_t add_stack_frames_in_table(stack_frames_db *db_context, uintptr_t *frames, int32_t count) {
    uint64_t stack_hash = __frames_hash(frames, count);
    uint32_t stack_identifier = 0;
    stack_entry_list_t *list = db_context->table_index->stack_entry_list[count - 1];

    // Check whether this stack exists
    if (list->exist(stack_hash)) {
        stack_identifier = list->find();
    } else {
        stack_identifier = __enter_frames_in_table(db_context->table, frames, count);
        list->insert(stack_hash, stack_identifier);
    }

#ifdef DEBUG
    // check stack collision
    uint64_t tmp[64];
    uint32_t out_frames_count = 0;
    static int64_t collise_count = 0;
    unwind_stack_from_table_index(db_context, stack_identifier, tmp, &out_frames_count, 64);
    if (out_frames_count != count) {
        collise_count++;
    } else {
        for (int i = 0; i < out_frames_count; ++i) {
            if (tmp[i] != frames[i]) {
                uint64_t stack_hash2 = __frames_hash((uintptr_t *)tmp, count);
                assert(stack_hash == stack_hash2);
                collise_count++;
                break;
            }
        }
    }
#endif

    if (!stack_identifier)
        abort();

    return stack_identifier;
}

void unwind_stack_from_table_index(
stack_frames_db *db_context, uint32_t stack_identifier, uint64_t *out_frames_buffer, uint32_t *out_frames_count, uint32_t max_frames) {
    table_slot_t *node = __get_node_from_table(db_context->table, stack_identifier);
    uint32_t foundFrames = 0;
    while (node && foundFrames < max_frames) {
        out_frames_buffer[foundFrames++] = node->detail.address;
        if (node->detail.uParent == 0)
            break;
        node = __get_node_from_table(db_context->table, node->detail.uParent);
        assert(foundFrames < max_frames || node == NULL);
    }

    *out_frames_count = foundFrames;
}
