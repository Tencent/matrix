/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef LIBMATRIX_JNI_UTILS_H
#define LIBMATRIX_JNI_UTILS_H

#include <cstdint>
#include <unwindstack/Unwinder.h>
#include "Backtrace.h"

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

uint64_t hash_uint64(uint64_t *p_pc_stacks, size_t stack_size);
// enhance: hash to 64 bits
uint64_t hash_backtrace_frames(wechat_backtrace::Backtrace *stack_frames);
uint64_t hash_str(const char * str);
uint64_t hash_combine(uint64_t l, uint64_t r);

/*
 * Estimated collision rate: 0.3%
 * Total 50173 hash found 74 hash collision involved 148 different backtrace.
 */
inline uint64_t hash_frames(wechat_backtrace::Frame *frame, size_t size) {
    if (unlikely(frame == nullptr || size == 0)) {
        return 1;
    }
    uint64_t sum = size;
    for (size_t i = 0; i < size; i++) {
        sum += frame[i].pc << (i << 1);
    }
    return (uint64_t) sum;
}


#endif //LIBMATRIX_JNI_UTILS_H
