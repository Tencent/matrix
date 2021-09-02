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

#include "allocation_event_db.h"
#include "buffer_source.h"
#include "splay_map_simple_key.h"

#ifdef ADDRESS_64

#pragma mark -
#pragma mark Types

#define ADDRESS_HASH_LV0_BITS 20
#define ADDRESS_HASH_LV1_BITS 12

#define ADDRESS_HASH_LV0_SIZE (1 << ADDRESS_HASH_LV0_BITS)
#define ADDRESS_HASH_LV1_SIZE (1 << ADDRESS_HASH_LV1_BITS)

// memory address is range of 0 ~ 2^36
struct address_hash {
    union {
        uint64_t address;

        struct {
            uint64_t reserve : 4;
            uint64_t lv1 : ADDRESS_HASH_LV1_BITS;
            uint64_t lv0 : ADDRESS_HASH_LV0_BITS;
            uint64_t space : 28;
        } detail;
    };
};

struct allocation_event_db {
    // tiny size(<= 512) event
    uint32_t key_hash[ADDRESS_HASH_LV0_SIZE];
    memory_pool_file *key_buffer_source0;

#ifdef DEBUG
    uint32_t val_count;
#endif
    allocation_event *val_free_ptr;
    allocation_event *val_page_curr;
    allocation_event *val_page_end;
    memory_pool_file *val_buffer_source0;

    // other size event
    buffer_source *key_buffer_source1;
    buffer_source *val_buffer_source1;
    splay_map_simple_key<uint64_t, allocation_event> *allocation_event_list;
};

#define EXPAND_KEY_POINTER(p) ((uint32_t *)((uintptr_t)p << 14))
#define EXPAND_VAL_POINTER(p) ((allocation_event *)((uintptr_t)p << 4))
#define VAL_ALLOC_COUNT (1 << 16)

static_assert(sizeof(address_hash) == sizeof(uint64_t), "Not aligned!");
static_assert(sizeof(allocation_event) == (1 << 4), "Not aligned!");

#pragma mark -
#pragma mark Public Interface

allocation_event_db *allocation_event_db_open_or_create(const char *event_dir) {
    allocation_event_db *db_context = (allocation_event_db *)inter_calloc(1, sizeof(allocation_event_db));
    db_context->key_buffer_source0 = new memory_pool_file(event_dir, "allocation_events_key0.dat");
    db_context->val_buffer_source0 = new memory_pool_file(event_dir, "allocation_events_val0.dat");

    if (db_context->key_buffer_source0->init_fail() || db_context->val_buffer_source0->init_fail()) {
        // should not be happened
        err_code = MS_ERRC_AE_FILE_OPEN_FAIL;
        goto init_fail;
    }

    db_context->key_buffer_source1 = new buffer_source_file(event_dir, "allocation_events_key1.dat");
    db_context->val_buffer_source1 = new buffer_source_file(event_dir, "allocation_events_val1.dat");
    if (db_context->key_buffer_source1->init_fail() || db_context->val_buffer_source1->init_fail()) {
        // should not be happened
        err_code = MS_ERRC_AE_FILE_OPEN_FAIL;
        goto init_fail;
    }

    db_context->allocation_event_list =
    new splay_map_simple_key<uint64_t, allocation_event>(VAL_ALLOC_COUNT, db_context->key_buffer_source1, db_context->val_buffer_source1);

    return db_context;

init_fail:
    allocation_event_db_close(db_context);
    return NULL;
}

void allocation_event_db_close(allocation_event_db *db_context) {
    if (db_context == NULL) {
        return;
    }

    for (int i = 0; i < ADDRESS_HASH_LV0_SIZE; ++i) {
        db_context->key_buffer_source0->free(EXPAND_KEY_POINTER(db_context->key_hash[i]), ADDRESS_HASH_LV1_SIZE * sizeof(uint32_t));
    }

    delete db_context->key_buffer_source0;
    delete db_context->val_buffer_source0;

    delete db_context->allocation_event_list;
    delete db_context->key_buffer_source1;
    delete db_context->val_buffer_source1;

    inter_free(db_context);
}

bool allocation_event_db_realloc_val(allocation_event_db *db_context) {
    uint32_t malloc_size = VAL_ALLOC_COUNT * sizeof(allocation_event);
    void *new_buff = db_context->val_buffer_source0->malloc(malloc_size);
    if (new_buff) {
        memset(new_buff, 0, malloc_size);
        db_context->val_page_curr = (allocation_event *)new_buff;
        db_context->val_page_end = (allocation_event *)((uintptr_t)new_buff + malloc_size);
        return true;
    } else {
        return false;
    }
}

void allocation_event_db_free_val(allocation_event_db *db_context, allocation_event *free_ptr) {
#ifdef DEBUG
    db_context->val_count--;
#endif
    free_ptr->alloca_type = memory_logging_type_free;
    *((allocation_event **)free_ptr) = db_context->val_free_ptr;
    db_context->val_free_ptr = free_ptr;
}

allocation_event *allocation_event_db_new_val(allocation_event_db *db_context) {
#ifdef DEBUG
    db_context->val_count++;
#endif
    allocation_event *new_ptr = db_context->val_free_ptr;
    if (new_ptr == NULL) {
        new_ptr = db_context->val_page_curr;
        if (new_ptr == db_context->val_page_end) {
            if (allocation_event_db_realloc_val(db_context) == false) {
                static allocation_event s_empty;
#ifdef DEBUG
                --db_context->val_count;
#endif
                return &s_empty;
            }
            new_ptr = db_context->val_page_curr;
        }
        db_context->val_page_curr++;
    } else {
        db_context->val_free_ptr = *((allocation_event **)db_context->val_free_ptr);
    }
    return new_ptr;
}

void __allocation_event_db_add(
allocation_event_db *db_context, uint64_t address, uint32_t type_flags, uint32_t object_type, uint32_t size, uint32_t stack_identifier) {
    address_hash ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv1 = ai.detail.lv1;

    uint32_t lv0_index = db_context->key_hash[lv0];
    if (lv0_index == 0) {
        uint32_t *new_buffer = (uint32_t *)db_context->key_buffer_source0->malloc(ADDRESS_HASH_LV1_SIZE * sizeof(uint32_t));
        if (new_buffer == NULL) {
            return;
        }
        memset(new_buffer, 0, ADDRESS_HASH_LV1_SIZE * sizeof(uint32_t));
        // Note: Large memory allocations are guaranteed to be page-aligned
        lv0_index = (uint32_t)((uintptr_t)new_buffer >> 14);
        db_context->key_hash[lv0] = lv0_index;
    }

    uint32_t *lv0_ptr = EXPAND_KEY_POINTER(lv0_index);
    uint32_t val_index = lv0_ptr[lv1];
    if (val_index == 0) {
        allocation_event *new_val = allocation_event_db_new_val(db_context);
        *new_val = allocation_event(type_flags, object_type, stack_identifier, size);
        lv0_ptr[lv1] = (uint32_t)((uintptr_t)new_val >> 4);
    } else {
        allocation_event *old_val = EXPAND_VAL_POINTER(val_index);
        *old_val = allocation_event(type_flags, object_type, stack_identifier, size);
    }
}

void allocation_event_db_add(
allocation_event_db *db_context, uint64_t address, uint32_t type_flags, uint32_t object_type, uint32_t size, uint32_t stack_identifier) {
    if ((type_flags & memory_logging_type_alloc) && /*(address & 0x1f) == 0 && */ size <= 128) {
        __allocation_event_db_add(db_context, address, type_flags, object_type, size, stack_identifier);
    } else {
        db_context->allocation_event_list->insert(address, allocation_event(type_flags, object_type, stack_identifier, size));
    }
}

bool __allocation_event_db_del(allocation_event_db *db_context, uint64_t address) {
    address_hash ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv0_index = db_context->key_hash[lv0];

    if (lv0_index) {
        uint32_t lv1 = ai.detail.lv1;
        uint32_t *lv0_ptr = EXPAND_KEY_POINTER(lv0_index);
        uint32_t val_index = lv0_ptr[lv1];
        if (val_index) {
            allocation_event_db_free_val(db_context, EXPAND_VAL_POINTER(val_index));
            lv0_ptr[lv1] = 0;
            return true;
        }
    }
    return false;
}

void allocation_event_db_del(allocation_event_db *db_context, uint64_t address, uint32_t type_flags) {
    if ((type_flags & memory_logging_type_dealloc) /* && (address & 0x1f) == 0*/) {
        if (__allocation_event_db_del(db_context, address)) {
            return;
        }
    }
    db_context->allocation_event_list->remove(address);
}

bool __allocation_event_db_update_object_type(allocation_event_db *db_context, uint64_t address, uint32_t new_type) {
    address_hash ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv0_index = db_context->key_hash[lv0];

    if (lv0_index) {
        uint32_t lv1 = ai.detail.lv1;
        uint32_t *lv0_ptr = EXPAND_KEY_POINTER(lv0_index);
        uint32_t val_index = lv0_ptr[lv1];
        if (val_index) {
            allocation_event *old_val = EXPAND_VAL_POINTER(val_index);
            old_val->object_type = new_type;
            return true;
        }
    }
    return false;
}

void allocation_event_db_update_object_type(allocation_event_db *db_context, uint64_t address, uint32_t new_type) {
    if (__allocation_event_db_update_object_type(db_context, address, new_type)) {
        return;
    }
    if (db_context->allocation_event_list->exist(address)) {
        db_context->allocation_event_list->find().object_type = new_type;
    }
}

bool __allocation_event_db_update_object_type_and_stack_identifier(allocation_event_db *db_context,
                                                                   uint64_t address,
                                                                   uint32_t new_type,
                                                                   uint32_t stack_identifier) {
    address_hash ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv0_index = db_context->key_hash[lv0];

    if (lv0_index) {
        uint32_t lv1 = ai.detail.lv1;
        uint32_t *lv0_ptr = EXPAND_KEY_POINTER(lv0_index);
        uint32_t val_index = lv0_ptr[lv1];
        if (val_index) {
            allocation_event *old_val = EXPAND_VAL_POINTER(val_index);
            old_val->object_type = new_type;
            old_val->stack_identifier = stack_identifier;
            return true;
        }
    }
    return false;
}

void allocation_event_db_update_object_type_and_stack_identifier(allocation_event_db *db_context,
                                                                 uint64_t address,
                                                                 uint32_t new_type,
                                                                 uint32_t stack_identifier) {
    if (__allocation_event_db_update_object_type_and_stack_identifier(db_context, address, new_type, stack_identifier)) {
        return;
    }
    if (db_context->allocation_event_list->exist(address)) {
        db_context->allocation_event_list->find().object_type = new_type;
        db_context->allocation_event_list->find().stack_identifier = stack_identifier;
    }
}

void allocation_event_db_enumerate(allocation_event_db *db_context, void (^callback)(const uint64_t &address, const allocation_event &event)) {
    if (db_context == NULL) {
        return;
    }

    size_t offset = 0;
    while (offset < db_context->val_buffer_source0->fs()) {
        size_t read_size = VAL_ALLOC_COUNT * sizeof(allocation_event);

        void *map_mem = inter_mmap(NULL, read_size, PROT_READ, MAP_SHARED, db_context->val_buffer_source0->fd(), offset);
        if (map_mem == MAP_FAILED) {
            continue;
        }

        allocation_event *val_buffer = (allocation_event *)map_mem;
        for (int i = 0; i < VAL_ALLOC_COUNT; ++i) {
            const allocation_event &event = val_buffer[i];
            if (event.alloca_type != memory_logging_type_free) {
                callback(1, event);
            }
        }

        inter_munmap(map_mem, read_size);

        offset += read_size;
    }

    db_context->allocation_event_list->enumerate(callback);
}

#else

#pragma mark -
#pragma mark Types

#define ADDRESS_HASH_LV0_BITS 20
#define ADDRESS_HASH_LV1_BITS 12

#define ADDRESS_HASH_LV0_SIZE (1 << ADDRESS_HASH_LV0_BITS)
#define ADDRESS_HASH_LV1_SIZE (1 << ADDRESS_HASH_LV1_BITS)

// memory address is range of 0 ~ 2^36
struct address_hash {
    union {
        uint32_t address;

        struct {
            //uint64_t reserve : 4;
            uint32_t lv1 : ADDRESS_HASH_LV1_BITS;
            uint32_t lv0 : ADDRESS_HASH_LV0_BITS;
            //uint64_t space : 28;
        } detail;
    };
};

struct allocation_event_db {
    // tiny size(<= 512) event
    uint32_t key_hash[ADDRESS_HASH_LV0_SIZE];
    memory_pool_file *key_buffer_source0;

#ifdef DEBUG
    uint32_t val_count;
#endif
    allocation_event *val_free_ptr;
    allocation_event *val_page_curr;
    allocation_event *val_page_end;
    memory_pool_file *val_buffer_source0;

    // other size event
    buffer_source *key_buffer_source1;
    buffer_source *val_buffer_source1;
    splay_map_simple_key<uint32_t, allocation_event> *allocation_event_list;
};

#define EXPAND_KEY_POINTER(p) ((uint32_t *)((uintptr_t)p << 14))
#define EXPAND_VAL_POINTER(p) ((allocation_event *)((uintptr_t)p << 4))
#define VAL_ALLOC_COUNT (1 << 16)

static_assert(sizeof(address_hash) == sizeof(uint32_t), "Not aligned!");
static_assert(sizeof(allocation_event) == (1 << 4), "Not aligned!");

#pragma mark -
#pragma mark Public Interface

allocation_event_db *allocation_event_db_open_or_create(const char *event_dir) {
    allocation_event_db *db_context = (allocation_event_db *)inter_calloc(1, sizeof(allocation_event_db));
    db_context->key_buffer_source0 = new memory_pool_file(event_dir, "allocation_events_key220.dat");
    db_context->val_buffer_source0 = new memory_pool_file(event_dir, "allocation_events_val220.dat");

    if (db_context->key_buffer_source0->init_fail() || db_context->val_buffer_source0->init_fail()) {
        // should not be happened
        err_code = MS_ERRC_AE_FILE_OPEN_FAIL;
        goto init_fail;
    }

    db_context->key_buffer_source1 = new buffer_source_file(event_dir, "allocation_events_key221.dat");
    db_context->val_buffer_source1 = new buffer_source_file(event_dir, "allocation_events_val221.dat");
    if (db_context->key_buffer_source1->init_fail() || db_context->val_buffer_source1->init_fail()) {
        // should not be happened
        err_code = MS_ERRC_AE_FILE_OPEN_FAIL;
        goto init_fail;
    }

    db_context->allocation_event_list =
    new splay_map_simple_key<uint32_t, allocation_event>(VAL_ALLOC_COUNT, db_context->key_buffer_source1, db_context->val_buffer_source1);

    return db_context;

init_fail:
    allocation_event_db_close(db_context);
    return NULL;
}

void allocation_event_db_close(allocation_event_db *db_context) {
    if (db_context == NULL) {
        return;
    }

    for (int i = 0; i < ADDRESS_HASH_LV0_SIZE; ++i) {
        db_context->key_buffer_source0->free(EXPAND_KEY_POINTER(db_context->key_hash[i]), ADDRESS_HASH_LV1_SIZE * sizeof(uint32_t));
    }

    delete db_context->key_buffer_source0;
    delete db_context->val_buffer_source0;

    delete db_context->allocation_event_list;
    delete db_context->key_buffer_source1;
    delete db_context->val_buffer_source1;

    inter_free(db_context);
}

bool allocation_event_db_realloc_val(allocation_event_db *db_context) {
    uint32_t malloc_size = VAL_ALLOC_COUNT * sizeof(allocation_event);
    void *new_buff = db_context->val_buffer_source0->malloc(malloc_size);
    if (new_buff) {
        memset(new_buff, 0, malloc_size);
        db_context->val_page_curr = (allocation_event *)new_buff;
        db_context->val_page_end = (allocation_event *)((uintptr_t)new_buff + malloc_size);
        return true;
    } else {
        return false;
    }
}

void allocation_event_db_free_val(allocation_event_db *db_context, allocation_event *free_ptr) {
#ifdef DEBUG
    db_context->val_count--;
#endif
    free_ptr->alloca_type = memory_logging_type_free;
    *((allocation_event **)free_ptr) = db_context->val_free_ptr;
    db_context->val_free_ptr = free_ptr;
}

allocation_event *allocation_event_db_new_val(allocation_event_db *db_context) {
#ifdef DEBUG
    db_context->val_count++;
#endif
    allocation_event *new_ptr = db_context->val_free_ptr;
    if (new_ptr == NULL) {
        new_ptr = db_context->val_page_curr;
        if (new_ptr == db_context->val_page_end) {
            if (allocation_event_db_realloc_val(db_context) == false) {
                static allocation_event s_empty;
#ifdef DEBUG
                --db_context->val_count;
#endif
                return &s_empty;
            }
            new_ptr = db_context->val_page_curr;
        }
        db_context->val_page_curr++;
    } else {
        db_context->val_free_ptr = *((allocation_event **)db_context->val_free_ptr);
    }
    return new_ptr;
}

void __allocation_event_db_add(
allocation_event_db *db_context, uint32_t address, uint32_t type_flags, uint32_t object_type, uint32_t size, uint32_t stack_identifier) {
    address_hash ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv1 = ai.detail.lv1;

    uint32_t lv0_index = db_context->key_hash[lv0];
    if (lv0_index == 0) {
        uint32_t *new_buffer = (uint32_t *)db_context->key_buffer_source0->malloc(ADDRESS_HASH_LV1_SIZE * sizeof(uint32_t));
        if (new_buffer == NULL) {
            return;
        }
        memset(new_buffer, 0, ADDRESS_HASH_LV1_SIZE * sizeof(uint32_t));
        // Note: Large memory allocations are guaranteed to be page-aligned
        lv0_index = (uint32_t)((uintptr_t)new_buffer >> 14);
        db_context->key_hash[lv0] = lv0_index;
    }

    uint32_t *lv0_ptr = EXPAND_KEY_POINTER(lv0_index);
    uint32_t val_index = lv0_ptr[lv1];
    if (val_index == 0) {
        allocation_event *new_val = allocation_event_db_new_val(db_context);
        *new_val = allocation_event(type_flags, object_type, stack_identifier, size);
        lv0_ptr[lv1] = (uint32_t)((uintptr_t)new_val >> 4);
    } else {
        allocation_event *old_val = EXPAND_VAL_POINTER(val_index);
        *old_val = allocation_event(type_flags, object_type, stack_identifier, size);
    }
}

void allocation_event_db_add(
allocation_event_db *db_context, uint64_t address, uint32_t type_flags, uint32_t object_type, uint32_t size, uint32_t stack_identifier) {
    uint32_t new_address = (uint32_t)(address >> 4);

    if ((type_flags & memory_logging_type_alloc) && /*(address & 0x1f) == 0 && */ size <= 128) {
        __allocation_event_db_add(db_context, new_address, type_flags, object_type, size, stack_identifier);
    } else {
        db_context->allocation_event_list->insert(new_address, allocation_event(type_flags, object_type, stack_identifier, size));
    }
}

bool __allocation_event_db_del(allocation_event_db *db_context, uint32_t address) {
    address_hash ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv0_index = db_context->key_hash[lv0];

    if (lv0_index) {
        uint32_t lv1 = ai.detail.lv1;
        uint32_t *lv0_ptr = EXPAND_KEY_POINTER(lv0_index);
        uint32_t val_index = lv0_ptr[lv1];
        if (val_index) {
            allocation_event_db_free_val(db_context, EXPAND_VAL_POINTER(val_index));
            lv0_ptr[lv1] = 0;
            return true;
        }
    }
    return false;
}

void allocation_event_db_del(allocation_event_db *db_context, uint64_t address, uint32_t type_flags) {
    uint32_t new_address = (uint32_t)(address >> 4);

    if ((type_flags & memory_logging_type_dealloc) /* && (address & 0x1f) == 0*/) {
        if (__allocation_event_db_del(db_context, new_address)) {
            return;
        }
    }
    db_context->allocation_event_list->remove(new_address);
}

bool __allocation_event_db_update_object_type(allocation_event_db *db_context, uint32_t address, uint32_t new_type) {
    address_hash ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv0_index = db_context->key_hash[lv0];

    if (lv0_index) {
        uint32_t lv1 = ai.detail.lv1;
        uint32_t *lv0_ptr = EXPAND_KEY_POINTER(lv0_index);
        uint32_t val_index = lv0_ptr[lv1];
        if (val_index) {
            allocation_event *old_val = EXPAND_VAL_POINTER(val_index);
            old_val->object_type = new_type;
            return true;
        }
    }
    return false;
}

void allocation_event_db_update_object_type(allocation_event_db *db_context, uint64_t address, uint32_t new_type) {
    uint32_t new_address = (uint32_t)(address >> 4);

    if (__allocation_event_db_update_object_type(db_context, new_address, new_type)) {
        return;
    }
    if (db_context->allocation_event_list->exist(new_address)) {
        db_context->allocation_event_list->find().object_type = new_type;
    }
}

bool __allocation_event_db_update_object_type_and_stack_identifier(allocation_event_db *db_context,
                                                                   uint32_t address,
                                                                   uint32_t new_type,
                                                                   uint32_t stack_identifier) {
    address_hash ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv0_index = db_context->key_hash[lv0];

    if (lv0_index) {
        uint32_t lv1 = ai.detail.lv1;
        uint32_t *lv0_ptr = EXPAND_KEY_POINTER(lv0_index);
        uint32_t val_index = lv0_ptr[lv1];
        if (val_index) {
            allocation_event *old_val = EXPAND_VAL_POINTER(val_index);
            old_val->object_type = new_type;
            old_val->stack_identifier = stack_identifier;
            return true;
        }
    }
    return false;
}

void allocation_event_db_update_object_type_and_stack_identifier(allocation_event_db *db_context,
                                                                 uint64_t address,
                                                                 uint32_t new_type,
                                                                 uint32_t stack_identifier) {
    uint32_t new_address = (uint32_t)(address >> 4);

    if (__allocation_event_db_update_object_type_and_stack_identifier(db_context, new_address, new_type, stack_identifier)) {
        return;
    }
    if (db_context->allocation_event_list->exist(new_address)) {
        db_context->allocation_event_list->find().object_type = new_type;
        db_context->allocation_event_list->find().stack_identifier = stack_identifier;
    }
}

void allocation_event_db_enumerate(allocation_event_db *db_context, void (^callback)(const uint64_t &address, const allocation_event &event)) {
    if (db_context == NULL) {
        return;
    }

    size_t offset = 0;
    while (offset < db_context->val_buffer_source0->fs()) {
        size_t read_size = VAL_ALLOC_COUNT * sizeof(allocation_event);

        void *map_mem = inter_mmap(NULL, read_size, PROT_READ, MAP_SHARED, db_context->val_buffer_source0->fd(), offset);
        if (map_mem == MAP_FAILED) {
            continue;
        }

        allocation_event *val_buffer = (allocation_event *)map_mem;
        for (int i = 0; i < VAL_ALLOC_COUNT; ++i) {
            const allocation_event &event = val_buffer[i];
            if (event.alloca_type != memory_logging_type_free) {
                callback(1, event);
            }
        }

        inter_munmap(map_mem, read_size);

        offset += read_size;
    }

    //db_context->allocation_event_list->enumerate(callback);
    db_context->allocation_event_list->enumerate(^(const unsigned int &key, const allocation_event &val) {
      callback((uint64_t)key << 4, val);
    });
}

#endif
