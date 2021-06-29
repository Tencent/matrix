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

#include "allocation_event_db_old.h"
#include "splay_map_simple_key.h"

#pragma mark -
#pragma mark Types

// Implememt by splay_map_simple_key
struct allocation_event_db_old {
    buffer_source *key_buffer;
    buffer_source *val_buffer;
    splay_map_simple_key<uint64_t, allocation_event> *allocation_event_list;
};

#pragma mark -
#pragma mark Public Interface

allocation_event_db_old *allocation_event_db_old_open_or_create(const char *event_dir) {
    allocation_event_db_old *db_context = (allocation_event_db_old *)inter_malloc(sizeof(allocation_event_db_old));
    db_context->key_buffer = new buffer_source_file(event_dir, "allocation_events_old_key.dat");
    db_context->val_buffer = new buffer_source_file(event_dir, "allocation_events_old_val.dat");

    if (db_context->key_buffer->init_fail() || db_context->val_buffer->init_fail()) {
        // should not be happened
        err_code = MS_ERRC_AE_FILE_OPEN_FAIL;
        goto init_fail;
    } else {
        db_context->allocation_event_list =
        new splay_map_simple_key<uint64_t, allocation_event>(102400, db_context->key_buffer, db_context->val_buffer);
    }

    return db_context;

init_fail:
    allocation_event_db_old_close(db_context);
    return NULL;
}

void allocation_event_db_old_close(allocation_event_db_old *db_context) {
    if (db_context == NULL) {
        return;
    }

    delete db_context->allocation_event_list;
    delete db_context->key_buffer;
    delete db_context->val_buffer;
    inter_free(db_context);
}

void allocation_event_db_old_add(
allocation_event_db_old *db_context, uint64_t address, uint32_t type_flags, uint32_t object_type, uint32_t size, uint32_t stack_identifier) {
    db_context->allocation_event_list->insert(address, allocation_event(type_flags, object_type, stack_identifier, size));
}

void allocation_event_db_old_del(allocation_event_db_old *db_context, uint64_t address, uint32_t type_flags) {
    db_context->allocation_event_list->remove(address);
}

void allocation_event_db_old_update_object_type(allocation_event_db_old *db_context, uint64_t address, uint32_t new_type) {
    // check if exist
    if (db_context->allocation_event_list->exist(address)) {
        db_context->allocation_event_list->find().object_type = new_type;
    }
}

void allocation_event_db_enumerate(allocation_event_db_old *db_context, void (^callback)(const uint64_t &address, const allocation_event &event)) {
    db_context->allocation_event_list->enumerate(callback);
}
