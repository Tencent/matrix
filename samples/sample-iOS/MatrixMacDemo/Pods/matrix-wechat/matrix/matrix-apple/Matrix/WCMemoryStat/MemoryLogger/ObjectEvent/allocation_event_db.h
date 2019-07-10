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

#ifndef allocation_event_db_h
#define allocation_event_db_h

#include <mach/mach.h>

struct allocation_event {
	uint64_t address; // top 8 bits are actually the flags
	uint16_t alloca_type; // allocation type, such as memory_logging_type_alloc or memory_logging_type_vm_allocate
	uint16_t object_type; // object type, such as NSObject, NSData, CFString, etc...
//	struct {
//		uint64_t address:48;
//		uint64_t object_type:16;
//	} detail;
	uint32_t stack_identifier;
	uint32_t size;
	uint32_t t_id;
	
	allocation_event(uint64_t _a=0, uint16_t _at=0, uint16_t _ot=0, uint32_t _si=0, uint32_t _sz=0, uint32_t _id=0) {
		address = _a;
		alloca_type = _at;
		object_type = _ot;
		stack_identifier = _si;
		size = _sz;
		t_id = _id;
	}
	
	inline bool operator != (const allocation_event &another) const {
		return address != another.address;
	}
	
	inline bool operator == (const allocation_event &another) const {
		return address == another.address;
	}
	
	inline bool operator > (const allocation_event &another) const {
		return address > another.address;
	}
	
	inline bool operator < (const allocation_event &another) const {
		return address < another.address;
	}
};

struct allocation_event_db;

allocation_event_db *open_or_create_allocation_event_db(const char *event_dir);
void close_allocation_event_db(allocation_event_db *db_context);

void add_allocation_event(allocation_event_db *db_context, uint64_t address, uint32_t type_flags, uint32_t object_type, uint32_t size, uint32_t stack_identifier, uint32_t t_id);
void del_allocation_event(allocation_event_db *db_context, uint64_t address, uint32_t type_flags);
void update_allocation_event_object_type(allocation_event_db *db_context, uint64_t address, uint32_t new_type);

void enumerate_allocation_event(allocation_event_db *db_context, void (^callback)(const allocation_event &event));

#endif /* allocation_event_db_h */
