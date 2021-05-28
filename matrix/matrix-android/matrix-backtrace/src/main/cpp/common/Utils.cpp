/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <cassert>
#include "Utils.h"
#include "Log.h"
#include "Backtrace.h"

#define JAVA_LONG_MAX_VALUE 0x7fffffffffffffff

BACKTRACE_EXPORT uint64_t hash_uint64(uint64_t *p_pc_stacks, size_t stack_size) {
    assert(p_pc_stacks != NULL && stack_size > 0);
    uint64_t sum = 0;
    for (size_t i = 0; i < stack_size; ++i) {
        sum += p_pc_stacks[i];
    }
    return sum;
}

BACKTRACE_EXPORT uint64_t hash_backtrace_frames(wechat_backtrace::Backtrace *backtrace) {
    uptr sum = 1;
    if (backtrace == nullptr) {
        return (uint64_t) sum;
    }
    for (size_t i = 0; i != backtrace->frame_size; i++) {
        sum += (backtrace->frames.get())[i].pc;
    }
    return (uint64_t) sum;
}

BACKTRACE_EXPORT uint64_t hash_str(const char * str) {
    if (!str) {
        LOGD("DEBUG", "str is NULL");
    }

    int seed = 31;
    uint64_t hash = 0;
    while (*str) {
        hash = hash * seed + (*str++);
    }
    return hash;
}

BACKTRACE_EXPORT uint64_t hash_combine(uint64_t l, uint64_t r) {
    return (l + r) & JAVA_LONG_MAX_VALUE;
}