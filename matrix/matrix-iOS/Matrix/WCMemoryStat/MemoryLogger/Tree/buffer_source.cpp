/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "buffer_source.h"

#pragma mark -
#pragma mark Defines

#define MALLOC_SIZE (1 << 20)

#pragma mark -
#pragma mark Constants/Globals

static memory_pool_file *s_pool = NULL;
static void *s_alloc_ptr;
static size_t s_alloc_index;

#pragma mark -
#pragma mark Functions

bool shared_memory_pool_file_init(const char *dir) {
    if (s_pool != NULL) {
        return false;
    }

    // remove old file
    remove_file(dir, "file_memory.dat");
    s_pool = new memory_pool_file(dir, "file_memory.dat");
    s_alloc_ptr = NULL;
    s_alloc_index = MALLOC_SIZE;
    return s_pool != NULL;
}

void *shared_memory_pool_file_alloc(size_t size) {
    if (s_alloc_index + size >= MALLOC_SIZE) {
        s_alloc_ptr = s_pool->malloc(MALLOC_SIZE);
        if (s_alloc_ptr == NULL) {
            abort();
        }
        s_alloc_index = 0;
    }

    void *ret = (void *)((uintptr_t)s_alloc_ptr + s_alloc_index);
    s_alloc_index += size;
    s_alloc_index = (((s_alloc_index + 15) >> 4) << 4);
    return ret;
}
