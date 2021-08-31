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
#include "MemoryHookMetas.h"
#include "common/Macros.h"

namespace matrix {

    void _hook_check(bool assertion) {
        HOOK_CHECK(assertion)
    }

#if USE_MEMORY_MESSAGE_QUEUE_LOCK_FREE == true
    std::atomic<size_t> g_queue_realloc_counter = 0;
    std::atomic<size_t> g_queue_realloc_size_counter = 0;
    std::atomic<size_t> g_queue_realloc_failure_counter = 0;
    std::atomic<size_t> g_queue_realloc_over_limit_counter = 0;
    std::atomic<size_t> g_queue_extra_stack_meta_allocated = 0;
    std::atomic<size_t> g_queue_extra_stack_meta_kept = 0;

#else
    std::atomic<size_t> BufferQueueContainer::g_locker_collision_counter = 0;

    std::atomic<size_t> BufferQueue::g_queue_realloc_memory_1_counter = 0;
    std::atomic<size_t> BufferQueue::g_queue_realloc_memory_2_counter = 0;
    std::atomic<size_t> BufferQueue::g_queue_realloc_reason_1_counter = 0;
    std::atomic<size_t> BufferQueue::g_queue_realloc_reason_2_counter = 0;
    std::atomic<size_t> BufferQueue::g_queue_realloc_failure_counter = 0;
    std::atomic<size_t> BufferQueue::g_queue_realloc_over_limit_counter = 0;
    std::atomic<size_t> BufferQueue::g_queue_extra_stack_meta_allocated = 0;
    std::atomic<size_t> BufferQueue::g_queue_extra_stack_meta_kept = 0;
#endif

    BufferManagement::BufferManagement(memory_meta_container *memory_meta_container) {

#if USE_MEMORY_MESSAGE_QUEUE_LOCK_FREE == true
        const size_t max_fold_ = 512;

        message_allocator_ = new ResizableFreeList<message_t, 9, max_fold_>();
        alloc_message_allocator_ = new ResizableFreeList<allocation_message_t, 8, max_fold_>();
        node_allocator_ = new ResizableFreeList<message_node_t *, 10, max_fold_>();
#endif

        containers_.reserve(MAX_PTR_SLOT);
        for (int i = 0; i < MAX_PTR_SLOT; ++i) {
            auto container = new BufferQueueContainer();
            container->queue_ = new BufferQueue(node_allocator_);
            containers_.emplace_back(container);
        }
        memory_meta_container_ = memory_meta_container;
    }

    BufferManagement::~BufferManagement() {

        for (auto container : containers_) {
            delete container;
        }

#if USE_MEMORY_MESSAGE_QUEUE_LOCK_FREE == true
        delete message_allocator_;
        delete alloc_message_allocator_;
        delete node_allocator_;
#else
        delete queue_swapped_;
#endif
    }

    [[noreturn]] void BufferManagement::process_routine(BufferManagement *this_) {
        while (true) {
            TEST_LOG_ERROR("Process routine outside ... this_->containers_ %zu",
                           this_->containers_.size());

#if USE_MEMORY_MESSAGE_QUEUE_LOCK_FREE != true
            if (!this_->queue_swapped_) this_->queue_swapped_ = new BufferQueue(SIZE_AUGMENT);
#endif

            size_t busy_queue = 0;
            for (auto container : this_->containers_) {
                TEST_LOG_ERROR("Process routine ... ");
#if USE_MEMORY_MESSAGE_QUEUE_LOCK_FREE != true
                BufferQueue *swapped = nullptr;
                {
                    std::lock_guard<std::mutex> lock(container->mutex_);
                    if (container->queue_ && !container->queue_->empty()) {
                        TEST_LOG_WARN("Swap queue ... ");
                        swapped = container->queue_;
                        container->queue_ = this_->queue_swapped_;
                    }
                }

                if (swapped && swapped->size() >= 5) {
                    busy_queue++;
                }
                if (swapped) {
                    TEST_LOG_WARN("Swapped ... ");
                    swapped->process(
                            [&](message_t *message, allocation_message_t *allocation_message) {
#else
                size_t message_counter = 0;
                if (container) {
                    container->queue_->process(
                            [&](Node<message_t> *message_node, Node<allocation_message_t> *allocation_message_node) {

                                message_t *message = &message_node->t_;
                                allocation_message_t *allocation_message = nullptr;
                                if (allocation_message_node) {
                                    allocation_message = &allocation_message_node->t_;
                                }
#endif
                                if (message->type == message_type_allocation ||
                                    message->type == message_type_mmap) {
#if USE_CRITICAL_CHECK == true
                                    HOOK_CHECK(allocation_message);
#else
                                    if (UNLIKELY(allocation_message == nullptr)) return;
#endif

#if USE_MEMORY_MESSAGE_QUEUE_LOCK_FREE == true
                                    message_counter++;
                                    if (message_counter >= 5) {
                                        busy_queue++;
                                    }
#endif

                                    uint64_t stack_hash = 0;
                                    if (allocation_message->backtrace.frame_size != 0) {
                                        stack_hash = hash_frames(
                                                allocation_message->backtrace.frames,
                                                allocation_message->backtrace.frame_size);
                                    }

                                    this_->memory_meta_container_->insert(
                                            reinterpret_cast<const void *>(allocation_message->ptr),
                                            stack_hash,
#if USE_SPLAY_MAP_SAVE_STACK == true
                                            allocation_message,
#endif
                                            [&](ptr_meta_t *ptr_meta, stack_meta_t *stack_meta) {
                                                ptr_meta->ptr = reinterpret_cast<void *>(allocation_message->ptr);
                                                ptr_meta->size = allocation_message->size;
                                                ptr_meta->attr.is_mmap =
                                                        message->type == message_type_mmap;

#if USE_STACK_HASH_NO_COLLISION == true
                                                if (UNLIKELY(!stack_meta)) {
                                                    ptr_meta->caller = allocation_message->caller;
                                                    return;
                                                }
#else
                                                ptr_meta->caller = allocation_message->caller;
                                                if (UNLIKELY(!stack_meta)) {
                                                    return;
                                                }
#endif

                                                stack_meta->size += allocation_message->size;
                                                if (stack_meta->backtrace.frame_size == 0 &&
                                                    allocation_message->backtrace.frame_size != 0) {
                                                    stack_meta->backtrace = allocation_message->backtrace;
                                                    stack_meta->caller = allocation_message->caller;
                                                }
                                            });
                                } else if (message->type == message_type_deletion ||
                                           message->type == message_type_munmap) {
                                    this_->memory_meta_container_->erase(
                                            reinterpret_cast<const void *>(message->ptr));
                                }
#if USE_MEMORY_MESSAGE_QUEUE_LOCK_FREE == true
                                if (message_node) {
                                    this_->message_allocator_->deallocate(message_node);
                                }
                                if (allocation_message_node) {
                                    this_->alloc_message_allocator_->deallocate(allocation_message_node);
                                }
                            });
                    if (message_counter > 0) {
                        TEST_LOG_ERROR("Processed ... %zu messages ", message_counter);
                    }
#else
                            });
                    swapped->reset();
                    this_->queue_swapped_ = swapped;
#endif
                }
            }

            float busy_ratio = ((float) busy_queue) / this_->containers_.size();

            if (busy_ratio > 0.9f) { // Super busy
                continue;
            } else if (busy_ratio > 0.6f) { // Busy
                usleep(PROCESS_BUSY_INTERVAL);
            } else if (busy_ratio > 0.3f) {
                usleep(PROCESS_NORMAL_INTERVAL);
            } else if (busy_ratio > 0.1f) {
                usleep(PROCESS_LESS_NORMAL_INTERVAL);
            } else {
                usleep(PROCESS_IDLE_INTERVAL);
            }
        }
    }

    void BufferManagement::start_process() {
        if (processing_) return;
        processing_ = true;
        pthread_create(&thread_, nullptr,
                       reinterpret_cast<void *(*)(void *)>(&BufferManagement::process_routine),
                       this);
        pthread_detach(thread_);
    }
}