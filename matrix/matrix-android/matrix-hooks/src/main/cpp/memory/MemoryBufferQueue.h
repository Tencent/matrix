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

#ifndef LIBMATRIX_HOOK_MEMORY_BUFFER_QUEUE_H
#define LIBMATRIX_HOOK_MEMORY_BUFFER_QUEUE_H

#include <unistd.h>
#include <map>
#include <memory>
#include "MemoryHookMetas.h"

#define SIZE_AUGMENT 192
#define PROCESS_BUSY_INTERVAL 40 * 1000L
#define PROCESS_NORMAL_INTERVAL 150 * 1000L
#define PROCESS_LESS_NORMAL_INTERVAL 300 * 1000L
#define PROCESS_IDLE_INTERVAL 800 * 1000L

#define MEMORY_OVER_LIMIT 1024 * 1024 * 200L    // 100M

#define POINTER_MASK 48

namespace matrix {

    typedef enum : uint8_t {
        message_type_nil = 0,
        message_type_allocation = 1,
        message_type_deletion = 2,
        message_type_mmap = 3,
        message_type_munmap = 4
    } message_type;

    typedef wechat_backtrace::BacktraceFixed<MEMHOOK_BACKTRACE_MAX_FRAMES> memory_backtrace_t;

    typedef struct {
        uintptr_t ptr;
        uintptr_t caller;
        size_t size;
        memory_backtrace_t backtrace{};
    } allocation_message_t;

#if defined(__aarch64__)
    typedef struct {
        message_type type: 8;
        union {
            uintptr_t index: 56;
            uintptr_t ptr: 56;
        };
    } message_t;
#else
    typedef struct __attribute__((__packed__)) {
        message_type type;
        union {
            uintptr_t index;
            uintptr_t ptr;
        };
    } message_t;
#endif

    static inline size_t memory_ptr_hash(uintptr_t _key) {
        return static_cast<size_t>((_key ^ (_key >> 16)) & PTR_MASK);
    }

    class BufferQueue {

    public:
        BufferQueue(size_t size_augment) {
            size_augment_ = size_augment;
            buffer_realloc_message_queue(true);
            buffer_realloc_alloc_queue(true);
        };

        ~BufferQueue() {
            if (message_queue_) {
                free(message_queue_);
                message_queue_ = nullptr;
            }
            if (alloc_message_queue_) {
                free(alloc_message_queue_);
                alloc_message_queue_ = nullptr;
            }
        };

        inline allocation_message_t *enqueue_allocation_message(bool is_mmap) {
            if (msg_queue_idx >= msg_queue_size_) {
                if (UNLIKELY(!buffer_realloc_message_queue(false))) {
                    return nullptr;
                }
            }
            if (alloc_msg_queue_idx >= alloc_queue_size_) {
                if (UNLIKELY(!buffer_realloc_alloc_queue(false))) {
                    return nullptr;
                }
            }

            message_t *msg_idx = &message_queue_[msg_queue_idx++];
            msg_idx->type = is_mmap ? message_type_mmap : message_type_allocation;
            msg_idx->index = alloc_msg_queue_idx;

            allocation_message_t *buffer = &alloc_message_queue_[alloc_msg_queue_idx++];
            *buffer = {};
            return buffer;
        }

        inline void enqueue_deletion_message(uintptr_t ptr, bool is_munmap) {

            // Fast path.
            if (msg_queue_idx > 0) {
                message_t *msg_idx = &message_queue_[msg_queue_idx - 1];
                if (((!is_munmap && msg_idx->type == message_type_allocation)
                     || (is_munmap && msg_idx->type == message_type_mmap))
                    && alloc_message_queue_[msg_idx->index].ptr == ptr) {
                    msg_idx->type = message_type_nil;
                    msg_queue_idx--;
                    alloc_msg_queue_idx--;
                    return;
                }
            }

            if (msg_queue_idx >= msg_queue_size_) {
                if (UNLIKELY(!buffer_realloc_message_queue(false))) {
                    return;
                }
            }

            message_t *msg_idx = &message_queue_[msg_queue_idx++];
            msg_idx->type = is_munmap ? message_type_munmap : message_type_deletion;
            msg_idx->ptr = ptr;
        }

        inline bool empty() const {
            return msg_queue_idx == 0;
        }

        inline void reset() {
            msg_queue_idx = 0;
            alloc_msg_queue_idx = 0;
            if (msg_queue_size_ > size_augment_ * 2) {
                buffer_realloc_message_queue(true);
            }
            if (alloc_queue_size_ > size_augment_) {
                buffer_realloc_alloc_queue(true);
            }
        }

        inline size_t size() {
            return msg_queue_idx;
        }

        inline void process(std::function<void(message_t *, allocation_message_t *)> callback) {
            for (size_t i = 0; i < msg_queue_idx; i++) {
                message_t *message = &message_queue_[i];
                allocation_message_t *allocation_message = nullptr;
                if (message->type == message_type_allocation ||
                    message->type == message_type_mmap) {
                    allocation_message = &alloc_message_queue_[message->index];
                }
                callback(message, allocation_message);
            }
        }

        // stat
        static std::atomic<size_t> g_queue_realloc_memory_1_counter;
        static std::atomic<size_t> g_queue_realloc_memory_2_counter;
        static std::atomic<size_t> g_queue_realloc_reason_1;
        static std::atomic<size_t> g_queue_realloc_reason_2;
        static std::atomic<size_t> g_queue_realloc_failure_counter;
        static std::atomic<size_t> g_queue_realloc_over_limit_counter;

    private:

        bool buffer_realloc_message_queue(bool init) {
            if (!init && msg_queue_size_ != 0) {
                g_queue_realloc_reason_1.fetch_add(1);
            }

            if (UNLIKELY(!init && g_queue_realloc_memory_1_counter.load() +
                                  g_queue_realloc_memory_2_counter.load() >= MEMORY_OVER_LIMIT)) {
                g_queue_realloc_over_limit_counter.fetch_add(1);
                return false;
            }

            size_t resize = msg_queue_size_;
            if (init) {
                resize = size_augment_ * 2;
            } else {
                resize += size_augment_ * 2; // TODO add __builtin_add_overflow check here
            }

            void *buffer = realloc(message_queue_, sizeof(message_t) * resize);

            if (buffer) {
                message_queue_ = static_cast<message_t *>(buffer);

                g_queue_realloc_memory_1_counter.fetch_sub(sizeof(message_t) * msg_queue_size_);
                msg_queue_size_ = resize;
                g_queue_realloc_memory_1_counter.fetch_add(sizeof(message_t) * msg_queue_size_);

                return true;
            } else {
                g_queue_realloc_failure_counter.fetch_add(1, std::memory_order_relaxed);

                return false;
            }
        }

        bool buffer_realloc_alloc_queue(bool init) {
            if (!init && alloc_queue_size_ != 0) {
                g_queue_realloc_reason_2.fetch_add(1);
            }

            if (UNLIKELY(!init && g_queue_realloc_memory_1_counter.load() +
                                  g_queue_realloc_memory_2_counter.load() >= MEMORY_OVER_LIMIT)) {
                g_queue_realloc_over_limit_counter.fetch_add(1);
                return false;
            }

            size_t resize = alloc_queue_size_;
            if (init) {
                resize = size_augment_;
            } else {
                resize += size_augment_; // TODO add __builtin_add_overflow check here
            }

            void *buffer = realloc(alloc_message_queue_,
                                   sizeof(allocation_message_t) * resize);
            if (buffer) {
                alloc_message_queue_ = static_cast<allocation_message_t *>(buffer);

                g_queue_realloc_memory_2_counter.fetch_sub(
                        sizeof(allocation_message_t) * alloc_queue_size_);
                alloc_queue_size_ = resize;
                g_queue_realloc_memory_2_counter.fetch_add(
                        sizeof(allocation_message_t) * alloc_queue_size_);

                return true;
            } else {
                g_queue_realloc_failure_counter.fetch_add(1, std::memory_order_relaxed);

                return false;
            }
        }

        size_t size_augment_ = 0;

        size_t msg_queue_size_ = 0;
        size_t msg_queue_idx = 0;
        message_t *message_queue_ = nullptr;

        size_t alloc_queue_size_ = 0;
        size_t alloc_msg_queue_idx = 0;
        allocation_message_t *alloc_message_queue_ = nullptr;

    };

    class BufferQueueContainer {
    public:
        BufferQueueContainer() : queue(nullptr) {}

        ~BufferQueueContainer() {
            delete queue;
        }

        BufferQueue *queue;
        std::mutex mutex;
    };

    class BufferManagement {
    public:
        BufferManagement(memory_meta_container *memory_meta_container);

        ~BufferManagement();

        void start_process();

        std::vector<BufferQueueContainer *> containers_;

    private:

        [[noreturn]] static void process_routine(BufferManagement *this_);

        BufferQueue *queue_swapped_ = nullptr;

        memory_meta_container *memory_meta_container_ = nullptr;

        bool processing_ = false;
        pthread_t thread_;
    };
}

#endif //LIBMATRIX_HOOK_MEMORY_BUFFER_QUEUE_H
