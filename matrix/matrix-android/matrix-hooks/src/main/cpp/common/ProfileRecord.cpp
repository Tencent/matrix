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
// Created by Yves on 2020-03-17.
//

#include <jni.h>
#include <sys/stat.h>
#include <pthread.h>
#include <backtrace/Backtrace.h>
#include <backtrace/BacktraceDefine.h>
#include <xhook.h>
#include <shared_mutex>
#include "Log.h"
#include "ProfileRecord.h"
#include "HookCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAG "Matrix.Record"

#define RECORD_LOGE(TAG, FMT, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

using namespace wechat_backtrace;

struct RecordMemoryChunk {
    uint64_t ptr;
    uint64_t size;
    uint64_t ts;
};

struct RecordMemoryBacktrace {
    uint64_t ptr;
    uint64_t ts;
    uint64_t frame_size;
    std::shared_ptr<wechat_backtrace::Frame> frames;
    uint64_t duration;
};

#define THREAD_MAX_COUNT 0x8000

static std::vector<RecordMemoryChunk>* thread_vectors[THREAD_MAX_COUNT] {};
static std::vector<RecordMemoryBacktrace>* thread_backtrace_vectors[THREAD_MAX_COUNT] {};

static std::shared_mutex thread_vectors_mutex_;

static bool dumped = false;

static std::vector<FakeRecordMemoryBacktrace>* fake_backtrace_vectors[THREAD_MAX_COUNT] {};
static uint64_t fake_backtrace_idx[THREAD_MAX_COUNT] {};

void set_fake_backtrace_vectors(std::vector<FakeRecordMemoryBacktrace>* v) {
    size_t tid = gettid();
    if (UNLIKELY(tid >= THREAD_MAX_COUNT)) { abort(); }
    fake_backtrace_vectors[tid] = v;
    fake_backtrace_idx[tid] = 0;
}

void fake_unwind(wechat_backtrace::Frame *frames, const size_t max_frames, size_t &frame_size) {
    size_t tid = gettid();
    if (UNLIKELY(tid >= THREAD_MAX_COUNT)) { abort(); }
    auto v = fake_backtrace_vectors[tid];
    if (!v) return;
    if (fake_backtrace_idx[tid] >= v->size()) return;
    auto backtrace = v->at(fake_backtrace_idx[tid]);
    size_t i = 0;
    size_t max_size = std::min(backtrace.frames->size(), max_frames);
    for (; i < max_size; i++) {
        frames[i].pc = (uptr) backtrace.frames->at(i);
    }

    frame_size = i;
    fake_backtrace_idx[tid] += 1;
}

void record_dump(const char *log_path) {
    std::unique_lock<std::shared_mutex> lock(thread_vectors_mutex_);

    if (dumped) {
        return;
    }

    dumped = true;

    RECORD_LOGE(TAG, ">>>>> record_dump <<<<<");

    FILE *log_file  = fopen(log_path, "w+");

    flogger0(log_file, "---\n");
    for (size_t i = 0; i < THREAD_MAX_COUNT; i++) {
        auto thread_vector = thread_vectors[i];
        auto thread_backtrace_vector = thread_backtrace_vectors[i];
        if (thread_vector) {
            RECORD_LOGE(TAG, ">>>>> thread_vector:%zu, %zu", i, thread_vector->size());
            flogger0(log_file, "# TID: %zu\n", i);
            for (auto chunk : *thread_vector) {
                flogger0(log_file, "%llu;%llu;%llu\n", (ullint_t) chunk.ts, (ullint_t) chunk.ptr, (ullint_t) chunk.size);
            }
            if (thread_backtrace_vector) {
                RECORD_LOGE(TAG, ">>>>> thread_backtrace_vector:%zu, %zu", i, thread_backtrace_vector->size());
                flogger0(log_file, "# TID backtrace: %zu\n", i);
                for (auto backtrace : *thread_backtrace_vector) {
                    flogger0(log_file, "%llu;%llu;%llu\n", (ullint_t) backtrace.ts, (ullint_t) backtrace.ptr, (ullint_t) backtrace.duration);
                    flogger0(log_file, "|%llu:", (ullint_t) backtrace.frame_size);
                    for (size_t idx = 0; idx < backtrace.frame_size; idx++) {
                        flogger0(log_file, "%llu;", backtrace.frames.get()[idx].pc);
                    }
                    flogger0(log_file, "\n");
                }
            }
            flogger0(log_file, "\n");
        }
    }
    flogger0(log_file, "---\n");

    if (log_file) {
        fflush(log_file);
        fclose(log_file);
    }
}

void record_on_memory_backtrace(uint64_t ptr, wechat_backtrace::Backtrace *backtrace, uint64_t duration) {
    std::shared_lock<std::shared_mutex> lock(thread_vectors_mutex_);

    size_t tid = gettid();
    if (UNLIKELY(tid >= THREAD_MAX_COUNT)) { abort(); }
    auto vector = thread_backtrace_vectors[tid];
    if (!vector) {
        vector = new std::vector<RecordMemoryBacktrace>();
        thread_backtrace_vectors[tid] = vector;
    }

    uint64_t timestamp = 0;
    struct timespec tms {};
    if (!clock_gettime(CLOCK_MONOTONIC, &tms)) {
        timestamp = tms.tv_nsec;
    }
    vector->push_back({
        .ptr = ptr,
        .ts = timestamp,
        .frame_size = backtrace->frame_size,
        .frames = backtrace->frames,
        .duration = duration
    });
}

void record_on_memory_allocated(uint64_t ptr, uint64_t size) {
    if (size == 0) return;

    std::shared_lock<std::shared_mutex> lock(thread_vectors_mutex_);

    size_t tid = gettid();
    if (UNLIKELY(tid >= THREAD_MAX_COUNT)) { abort(); }
    auto vector = thread_vectors[tid];
    if (!vector) {
        vector = new std::vector<RecordMemoryChunk>();
        thread_vectors[tid] = vector;
    }

    uint64_t timestamp = 0;
    struct timespec tms {};
    if (!clock_gettime(CLOCK_MONOTONIC, &tms)) {
        timestamp = tms.tv_nsec;
    }
    vector->push_back({
        .ptr = ptr,
        .size = size,
        .ts = timestamp
    });
}

void record_on_memory_released(uint64_t ptr) {

    std::shared_lock<std::shared_mutex> lock(thread_vectors_mutex_);

    size_t tid = gettid();
    if (UNLIKELY(tid >= THREAD_MAX_COUNT)) { abort(); }
    auto vector = thread_vectors[tid];
    if (!vector) {
        vector = new std::vector<RecordMemoryChunk>();
        thread_vectors[tid] = vector;
    }

    size_t timestamp = 0;
    struct timespec tms {};
    if (!clock_gettime(CLOCK_MONOTONIC, &tms)) {
        timestamp = tms.tv_nsec;
    }
    vector->push_back({
        .ptr = ptr,
        .size = 0,
        .ts = timestamp
    });
}

void record_on_so_loaded(const char * so) {

//    size_t tid = gettid();
//    if (!LIKELY(tid <= THREAD_MAX_COUNT)) { abort(); }
//    auto vector = thread_vectors[tid];
//    if (!vector) {
//        vector = new std::vector<RecordMemoryChunk>();
//        thread_vectors[tid] = vector;
//    }
//
//    size_t timestamp = 0;
//    struct timespec tms {};
//    if (!clock_gettime(CLOCK_MONOTONIC, &tms)) {
//        timestamp = tms.tv_nsec;
//    }
//
//    size_t len = strlen(so);
//    auto so_name = malloc(len + 1);
//    memcpy(so_name, so, len);
//    so_name[len] = '\0';
//
//    vector->push_back({
//        .ptr = (uptr) so_name,
//        .size = 0xFFFFFFFF,
//        .ts = timestamp
//    });
}

#ifdef __cplusplus
}

#undef TAG

#endif