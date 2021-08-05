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

//
// Created by Yves on 2019-12-16.
//
// must define ORIGINAL_LIB to use CALL_ORIGIN_FUNC_RET and CALL_ORIGIN_FUNC_VOID
//

#ifndef LIBMATRIX_PROFILE_RECORD_H
#define LIBMATRIX_PROFILE_RECORD_H

#include "Macros.h"
#include "backtrace/BacktraceDefine.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FakeRecordMemoryBacktrace {
    uint64_t ptr;
    uint64_t ts;
    uint64_t frame_size;
    std::vector<uint64_t>* frames;
    uint64_t duration;
};


EXPORT void set_fake_backtrace_vectors(std::vector<FakeRecordMemoryBacktrace>* v);
EXPORT void fake_unwind(wechat_backtrace::Frame *frames, const size_t max_frames, size_t &frame_size);

#ifdef RECORD_MEMORY_OP

#define ON_RECORD_START(timestamp) \
        long timestamp = 0; \
        { \
            struct timespec tms {}; \
            if (!clock_gettime(CLOCK_MONOTONIC, &tms)) { \
                timestamp = tms.tv_nsec; \
            } \
        }

#define ON_RECORD_END(timestamp) \
        if (timestamp > 0) { \
            struct timespec tms {}; \
            if (!clock_gettime(CLOCK_MONOTONIC, &tms)) { \
                timestamp = (tms.tv_nsec - timestamp); \
            } else { \
                timestamp = 0; \
            } \
        }

EXPORT void record_dump(const char *log_path);
EXPORT void record_on_memory_backtrace(uint64_t ptr, wechat_backtrace::Backtrace *backtrace, uint64_t durations);
EXPORT void record_on_memory_allocated(uint64_t ptr, uint64_t size);
EXPORT void record_on_memory_released(uint64_t ptr);
EXPORT void record_on_so_loaded(const char * so);

#define DUMP_RECORD(p) record_dump(p)
#define ON_MEMORY_BACKTRACE(x, b, d) record_on_memory_backtrace(x, b, d)
#define ON_MEMORY_ALLOCATED(x, s) record_on_memory_allocated(x, s)
#define ON_MEMORY_RELEASED(x) record_on_memory_released(x)
#define ON_SO_LOADED(x) record_on_so_loaded(x)
#else
#define ON_RECORD_START(timestamp)
#define ON_RECORD_END(timestamp)
#define DUMP_RECORD(p)
#define ON_MEMORY_BACKTRACE(x, b, d)
#define ON_MEMORY_ALLOCATED(x, s)
#define ON_MEMORY_RELEASED(x)
#define ON_SO_LOADED(x)
#endif

#ifdef __cplusplus
}
#endif

#endif //LIBMATRIX_PROFILE_RECORD_H
