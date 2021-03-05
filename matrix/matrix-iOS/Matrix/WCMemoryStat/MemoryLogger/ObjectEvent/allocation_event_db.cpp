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

#pragma mark -
#pragma mark Types

#define ADDRESS_HASH16_LV0_BITS 20
#define ADDRESS_HASH16_LV1_BITS 12

#define ADDRESS_HASH16_LV0_SIZE (1 << ADDRESS_HASH16_LV0_BITS)
#define ADDRESS_HASH16_LV1_SIZE (1 << ADDRESS_HASH16_LV1_BITS)

struct address_hash16 {
    union {
        uint64_t address;

        struct {
            uint64_t reserve : 4;
            uint64_t lv1 : ADDRESS_HASH16_LV1_BITS;
            uint64_t lv0 : ADDRESS_HASH16_LV0_BITS;
            uint64_t space : 28;
        } detail;
    };
};

#define ADDRESS_HASH32_LV0_BITS 19
#define ADDRESS_HASH32_LV1_BITS 12

#define ADDRESS_HASH32_LV0_SIZE (1 << ADDRESS_HASH32_LV0_BITS)
#define ADDRESS_HASH32_LV1_SIZE (1 << ADDRESS_HASH32_LV1_BITS)

struct address_hash32 {
    union {
        uint64_t address;

        struct {
            uint64_t reserve : 5;
            uint64_t lv1 : ADDRESS_HASH32_LV1_BITS;
            uint64_t lv0 : ADDRESS_HASH32_LV0_BITS;
            uint64_t space : 28;
        } detail;
    };
};

struct allocation_event_db {
    uint32_t *key_hash16[ADDRESS_HASH16_LV0_SIZE];
    uint32_t *key_hash32[ADDRESS_HASH32_LV0_SIZE];
    memory_pool_file *key_buffer_source;

    uint32_t val_free_index;
    uint32_t val_count;
    uint32_t val_max_count;
    allocation_event *val_buffer;
    buffer_source *val_buffer_source;
};

static_assert(sizeof(address_hash16) == sizeof(uint64_t), "Not aligned!");
static_assert(sizeof(address_hash32) == sizeof(uint64_t), "Not aligned!");

#pragma mark -
#pragma mark Public Interface

allocation_event_db *allocation_event_db_open_or_create(const char *event_dir) {
    allocation_event_db *db_context = (allocation_event_db *)inter_calloc(1, sizeof(allocation_event_db));
    db_context->key_buffer_source = new memory_pool_file(event_dir, "allocation_events_key.dat");
    db_context->val_buffer_source = new buffer_source_file(event_dir, "allocation_events_val.dat");

    if (db_context->key_buffer_source->init_fail() || db_context->val_buffer_source->init_fail()) {
        // should not be happened
        err_code = MS_ERRC_AE_FILE_OPEN_FAIL;
        goto init_fail;
    }

    db_context->val_buffer = (allocation_event *)db_context->val_buffer_source->buffer();
    db_context->val_max_count = (uint32_t)(db_context->val_buffer_source->buffer_size() / sizeof(allocation_event));

    return db_context;

init_fail:
    allocation_event_db_close(db_context);
    return NULL;
}

void allocation_event_db_close(allocation_event_db *db_context) {
    if (db_context == NULL) {
        return;
    }

    for (int i = 0; i < ADDRESS_HASH16_LV0_SIZE; ++i) {
        db_context->key_buffer_source->free(db_context->key_hash16[i], ADDRESS_HASH16_LV1_SIZE * sizeof(uint32_t));
    }

    for (int i = 0; i < ADDRESS_HASH32_LV0_SIZE; ++i) {
        db_context->key_buffer_source->free(db_context->key_hash32[i], ADDRESS_HASH32_LV1_SIZE * sizeof(uint32_t));
    }

    delete db_context->key_buffer_source;
    delete db_context->val_buffer_source;
    inter_free(db_context);
}

void allocation_event_db_realloc_val(allocation_event_db *db_context) {
    uint32_t malloc_size = (db_context->val_max_count + 102400) * sizeof(allocation_event);
    void *new_buff = db_context->val_buffer_source->realloc(malloc_size);
    if (new_buff) {
        memset((char *)new_buff + db_context->val_max_count * sizeof(allocation_event), 0, 102400 * sizeof(allocation_event));
        db_context->val_buffer = (allocation_event *)new_buff;
        db_context->val_max_count += 102400;
    }
}

void allocation_event_db_free_val(allocation_event_db *db_context, uint32_t index) {
    db_context->val_buffer[index].size = db_context->val_free_index;
    db_context->val_buffer[index].alloca_type = memory_logging_type_free;
    db_context->val_free_index = index;
}

uint32_t allocation_event_db_new_val(allocation_event_db *db_context) {
    uint32_t next_index = db_context->val_free_index;
    if (next_index == 0) {
        next_index = ++db_context->val_count;
        if (next_index >= db_context->val_max_count) {
            allocation_event_db_realloc_val(db_context);
        }
    } else {
        db_context->val_free_index = db_context->val_buffer[next_index].size;
    }
    return next_index;
}

void allocation_event_db_add16(allocation_event_db *db_context,
                               uint64_t address,
                               uint32_t type_flags,
                               uint32_t object_type,
                               uint32_t size,
                               uint32_t stack_identifier,
                               uint32_t t_id) {
    address_hash16 ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv1 = ai.detail.lv1;

    if (db_context->key_hash16[lv0] == NULL) {
        uint32_t *new_buffer = (uint32_t *)db_context->key_buffer_source->malloc(ADDRESS_HASH16_LV1_SIZE * sizeof(uint32_t));
        memset(new_buffer, 0, ADDRESS_HASH16_LV1_SIZE * sizeof(uint32_t));
        db_context->key_hash16[lv0] = new_buffer;
    }

    uint32_t old_index = db_context->key_hash16[lv0][lv1];
    if (old_index == 0) {
        uint32_t new_val_index = allocation_event_db_new_val(db_context);
        db_context->key_hash16[lv0][lv1] = new_val_index;
        db_context->val_buffer[new_val_index] = allocation_event(type_flags, object_type, stack_identifier, size, t_id);
    } else {
        db_context->val_buffer[old_index] = allocation_event(type_flags, object_type, stack_identifier, size, t_id);
    }
}

void allocation_event_db_add32(allocation_event_db *db_context,
                               uint64_t address,
                               uint32_t type_flags,
                               uint32_t object_type,
                               uint32_t size,
                               uint32_t stack_identifier,
                               uint32_t t_id) {
    address_hash32 ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv1 = ai.detail.lv1;

    if (db_context->key_hash32[lv0] == NULL) {
        uint32_t *new_buffer = (uint32_t *)db_context->key_buffer_source->malloc(ADDRESS_HASH32_LV1_SIZE * sizeof(uint32_t));
        memset(new_buffer, 0, ADDRESS_HASH32_LV1_SIZE * sizeof(uint32_t));
        db_context->key_hash32[lv0] = new_buffer;
    }

    uint32_t old_index = db_context->key_hash32[lv0][lv1];
    if (old_index == 0) {
        uint32_t new_val_index = allocation_event_db_new_val(db_context);
        db_context->key_hash32[lv0][lv1] = new_val_index;
        db_context->val_buffer[new_val_index] = allocation_event(type_flags, object_type, stack_identifier, size, t_id);
    } else {
        db_context->val_buffer[old_index] = allocation_event(type_flags, object_type, stack_identifier, size, t_id);
    }
}

void allocation_event_db_add(allocation_event_db *db_context,
                             uint64_t address,
                             uint32_t type_flags,
                             uint32_t object_type,
                             uint32_t size,
                             uint32_t stack_identifier,
                             uint32_t t_id) {
    if ((address & 0x1f) == 0) {
        allocation_event_db_add32(db_context, address, type_flags, object_type, size, stack_identifier, t_id);
    } else {
        allocation_event_db_add16(db_context, address, type_flags, object_type, size, stack_identifier, t_id);
    }
}

void allocation_event_db_del16(allocation_event_db *db_context, uint64_t address, uint32_t type_flags) {
    address_hash16 ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv1 = ai.detail.lv1;

    if (db_context->key_hash16[lv0]) {
        uint32_t val_index = db_context->key_hash16[lv0][lv1];
        if (val_index) {
            allocation_event_db_free_val(db_context, val_index);
            db_context->key_hash16[lv0][lv1] = 0;
        }
    }
}

void allocation_event_db_del32(allocation_event_db *db_context, uint64_t address, uint32_t type_flags) {
    address_hash32 ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv1 = ai.detail.lv1;

    if (db_context->key_hash32[lv0]) {
        uint32_t val_index = db_context->key_hash32[lv0][lv1];
        if (val_index) {
            allocation_event_db_free_val(db_context, val_index);
            db_context->key_hash32[lv0][lv1] = 0;
        }
    }
}

void allocation_event_db_del(allocation_event_db *db_context, uint64_t address, uint32_t type_flags) {
    if ((address & 0x1f) == 0) {
        allocation_event_db_del32(db_context, address, type_flags);
    } else {
        allocation_event_db_del16(db_context, address, type_flags);
    }
}

void allocation_event_db_update_object_type16(allocation_event_db *db_context, uint64_t address, uint32_t new_type) {
    address_hash16 ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv1 = ai.detail.lv1;

    if (db_context->key_hash16[lv0]) {
        uint32_t val_index = db_context->key_hash16[lv0][lv1];
        if (val_index) {
            db_context->val_buffer[val_index].object_type = new_type;
        }
    }
}

void allocation_event_db_update_object_type32(allocation_event_db *db_context, uint64_t address, uint32_t new_type) {
    address_hash32 ai = { .address = address };
    uint32_t lv0 = ai.detail.lv0;
    uint32_t lv1 = ai.detail.lv1;

    if (db_context->key_hash32[lv0]) {
        uint32_t val_index = db_context->key_hash32[lv0][lv1];
        if (val_index) {
            db_context->val_buffer[val_index].object_type = new_type;
        }
    }
}

void allocation_event_db_update_object_type(allocation_event_db *db_context, uint64_t address, uint32_t new_type) {
    if ((address & 0x1f) == 0) {
        allocation_event_db_update_object_type32(db_context, address, new_type);
    } else {
        allocation_event_db_update_object_type16(db_context, address, new_type);
    }
}

void allocation_event_db_enumerate(allocation_event_db *db_context, void (^callback)(const uint64_t &address, const allocation_event &event)) {
    if (db_context == NULL || db_context->val_buffer == NULL) {
        return;
    }

    for (int i = 0; i < db_context->val_max_count; ++i) {
        const allocation_event &event = db_context->val_buffer[i];
        if (event.alloca_type != memory_logging_type_free) {
            callback(1, event);
        }
    }
}
