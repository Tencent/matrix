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

#ifndef allocation_event_db_old_h
#define allocation_event_db_old_h

#include "allocation_event.h"

struct allocation_event_db_old;

allocation_event_db_old *allocation_event_db_old_open_or_create(const char *event_dir);
void allocation_event_db_old_close(allocation_event_db_old *db_context);

void allocation_event_db_old_add(
allocation_event_db_old *db_context, uint64_t address, uint32_t type_flags, uint32_t object_type, uint32_t size, uint32_t stack_identifier);
void allocation_event_db_old_del(allocation_event_db_old *db_context, uint64_t address, uint32_t type_flags);
void allocation_event_db_old_update_object_type(allocation_event_db_old *db_context, uint64_t address, uint32_t new_type);

void allocation_event_db_old_enumerate(allocation_event_db_old *db_context, void (^callback)(const uint64_t &address, const allocation_event &event));

#endif /* allocation_event_db_old_h */
