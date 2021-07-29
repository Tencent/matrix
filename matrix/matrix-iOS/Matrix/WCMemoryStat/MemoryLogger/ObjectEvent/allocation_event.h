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

#ifndef allocation_event_h
#define allocation_event_h

#include <mach/mach.h>

struct allocation_event {
    uint32_t stack_identifier;
    uint32_t size;
    uint32_t object_type; // object type, such as NSObject, NSData, CFString, etc...
    uint32_t alloca_type; // allocation type, such as memory_logging_type_alloc or memory_logging_type_vm_allocate

    allocation_event(uint32_t _at = 0, uint32_t _ot = 0, uint32_t _si = 0, uint32_t _sz = 0) {
        alloca_type = _at;
        object_type = _ot;
        stack_identifier = _si;
        size = _sz;
    }
};

#endif /* allocation_event_h */
