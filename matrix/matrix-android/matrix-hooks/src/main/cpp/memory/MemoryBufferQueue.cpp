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

#include <malloc.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <list>
#include <map>
#include <memory>
#include <thread>
#include <random>
#include <xhook.h>
#include <sstream>
#include <cxxabi.h>
#include <sys/mman.h>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <cJSON.h>
#include <Log.h>
#include "MemoryBufferQueue.h"
#include "common/Macros.h"

#define TEST_LOG_WARN(fmt, ...) __android_log_print(ANDROID_LOG_WARN,  "TestHook", fmt, ##__VA_ARGS__)

namespace matrix {

    std::atomic<size_t> BufferQueue::g_queue_realloc_memory_1_counter = 0;
    std::atomic<size_t> BufferQueue::g_queue_realloc_memory_2_counter = 0;
    std::atomic<size_t> BufferQueue::g_queue_realloc_reason_1 = 0;
    std::atomic<size_t> BufferQueue::g_queue_realloc_reason_2 = 0;

    // ---------- BufferManagement ----------
    BufferManagement::BufferManagement(memory_meta_container * memory_meta_container) {
        containers_.reserve(MAX_PTR_SLOT);
        for (int i = 0; i < MAX_PTR_SLOT; ++i) {
            containers_.emplace_back(new BufferQueueContainer());
        }
        memory_meta_container_ = memory_meta_container;
    }

    BufferManagement::~BufferManagement() {
        for (auto container : containers_) {
            delete container;
        }

        delete queue_swapped_;
    }

    [[noreturn]] void BufferManagement::process_routine(BufferManagement *this_) {
        while (true) {
//        TEST_LOG_WARN("Process routine outside ... this_->containers_ %zu", this_->containers_.size());
            if (!this_->queue_swapped_) this_->queue_swapped_ = new BufferQueue(SIZE_AUGMENT);

            for (auto container : this_->containers_) {
//            TEST_LOG_WARN("Process routine ... ");
                BufferQueue *swapped = nullptr;
                {
                    std::lock_guard<std::mutex> lock(container->mutex);
                    if (container->queue && !container->queue->empty()) {
//                    TEST_LOG_WARN("Swap queue ... ");
                        swapped = container->queue;
                        container->queue = this_->queue_swapped_;
                    }
                }
                if (swapped) {
//                TEST_LOG_WARN("Swapped ... ");
                    swapped->process([&](message_t *message, allocation_message_t *allocation_message) {
                        if (message->type == message_type_allocation ||
                            message->type == message_type_mmap) {
                            if (UNLIKELY(allocation_message == nullptr)) return;
                            uint64_t stack_hash = 0;
                            if (allocation_message->backtrace.frame_size != 0) {
                                stack_hash = hash_frames(allocation_message->backtrace.frames,
                                                         allocation_message->backtrace.frame_size);
                            }
                            this_->memory_meta_container_->insert(
                                    reinterpret_cast<const void *>(allocation_message->ptr),
                                    stack_hash,
                                    [&](ptr_meta_t *ptr_meta, stack_meta_t *stack_meta) {
                                        ptr_meta->ptr = reinterpret_cast<void *>(allocation_message->ptr);
                                        ptr_meta->size = allocation_message->size;
                                        ptr_meta->caller = reinterpret_cast<void *>(allocation_message->caller);
                                        ptr_meta->is_mmap = message->type == message_type_mmap;

                                        if (UNLIKELY(!stack_meta)) {
                                            return;
                                        }

                                        stack_meta->size += allocation_message->size;
                                        if (stack_meta->backtrace.frame_size == 0 &&
                                            allocation_message->backtrace.frame_size != 0) {
                                            stack_meta->backtrace = allocation_message->backtrace;
                                            stack_meta->caller = reinterpret_cast<void *>(allocation_message->caller);
                                        }
                                    });
                        } else if (message->type == message_type_deletion ||
                                   message->type == message_type_munmap) {
                            this_->memory_meta_container_->erase(
                                    reinterpret_cast<const void *>(message->ptr));
                        }
                    });

                    swapped->reset();
                    this_->queue_swapped_ = swapped;
                }
            }

            usleep(PROCESS_INTERVAL);
        }
    }

    void BufferManagement::start_process() {
        if (processing_) return;
        processing_ = true;
        pthread_t thread;
        pthread_create(&thread, nullptr, reinterpret_cast<void *(*)(void *)>(&BufferManagement::process_routine), this);
//    pthread_detach(thread);   // TODO
    }
}