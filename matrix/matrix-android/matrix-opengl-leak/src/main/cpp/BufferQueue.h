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
#include "../../../../matrix-hooks/src/main/cpp/common/Macros.h"
#include "functional"

namespace matrix {

    __attribute__((noinline)) void _hook_check(bool assertion);

    static const unsigned int MAX_PTR_SLOT = 1 << 8;
    static const unsigned int PTR_MASK = MAX_PTR_SLOT - 1;

    struct message_t {
        std::function<void()> runnable;
    };

    static inline size_t container_hash(uintptr_t _key) {
        return static_cast<size_t>((_key ^ (_key >> 16)) & PTR_MASK);
    }

    typedef struct {
        std::atomic<size_t> realloc_memory_counter_;
        std::atomic<size_t> realloc_counter_;
        std::atomic<size_t> realloc_failure_counter_;
        std::atomic<size_t> realloc_reach_limit_counter_;
    } message_queue_counter_t;

    class MessageAllocator {
    public:
        MessageAllocator(
                const size_t size_augment,
                const size_t memory_limit,
                message_queue_counter_t *const counter)
                : size_augment_(size_augment), memory_limit_(memory_limit), counter_(counter) {
            HOOK_CHECK(size_augment);
            HOOK_CHECK(memory_limit);
            HOOK_CHECK(counter);
        };

        ~MessageAllocator() {
            free(queue_);
        };

        message_queue_counter_t *const counter_;

        const size_t size_augment_;
        const size_t memory_limit_;

        size_t size_ = 0;
        size_t idx_ = 0;
        message_t *queue_ = nullptr;

        inline void reset() {
            idx_ = 0;
            if (size_ > size_augment_) {
                buffer_realloc(true);
            }
        }

        inline bool check_realloc() {
            if (UNLIKELY(idx_ >= size_)) {
                if (UNLIKELY(!buffer_realloc(false))) {
                    return false;
                }
            }

            return true;
        }

        inline bool buffer_realloc(bool init) {
            if (UNLIKELY(!init &&
                         counter_->realloc_memory_counter_.load(std::memory_order_relaxed) >=
                         memory_limit_)) {
                counter_->realloc_reach_limit_counter_.fetch_add(1, std::memory_order_relaxed);
                return false;
            }

            if (!init && size_ != 0) {
                counter_->realloc_counter_.fetch_add(1, std::memory_order_relaxed);
            }

            size_t resize = init ? size_augment_ : (size_ + size_augment_);

            void *buffer = calloc(resize, sizeof(message_t));

            if (LIKELY(buffer)) {
                if (!init) {
                    message_t *copy = static_cast<message_t *>(buffer);
                    for (int i = 0; i < size_; ++i) {
                        copy[i] = message_t();
                        copy[i].runnable = queue_[i].runnable;
                        queue_[i].runnable = nullptr;
                    }
                    free(queue_);
                }

                if (init && size_ != 0) {
                    for (int i = 0; i < size_; ++i) {
                        queue_[i].runnable = nullptr;
                    }
                    free(queue_);
                }

                queue_ = static_cast<message_t *>(buffer);

                counter_->realloc_memory_counter_.fetch_sub(sizeof(message_t) * size_);
                size_ = resize;
                counter_->realloc_memory_counter_.fetch_add(sizeof(message_t) * size_);
                return true;
            } else {
                counter_->realloc_failure_counter_.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }
    };

    class BufferQueue {

    public:
        BufferQueue(size_t size_augment) {
            const size_t limit_size = (MEMORY_OVER_LIMIT / (sizeof(message_t) * 2));

            messages_ = new MessageAllocator(
                    size_augment * 2,
                    limit_size * 2 * sizeof(message_t),
                    &g_message_queue_counter_);
            messages_->buffer_realloc(true);
        };

        ~BufferQueue() {
            delete messages_;
        };

        void enqueue_message(std::function<void()> runnable) {
            if (UNLIKELY(!messages_->check_realloc())) {
                return;
            }

            message_t *message = &messages_->queue_[messages_->idx_++];
            message->runnable = runnable;
        }

        bool empty() const {
            return messages_->idx_ == 0;
        }

        void reset() {
            messages_->reset();
        }

        size_t size() {
            return messages_->idx_;
        }

        void process(std::function<void(message_t *)> callback) {
            for (size_t i = 0; i < messages_->idx_; i++) {
                message_t *message = reinterpret_cast<message_t *>(&messages_->queue_[i]);
                callback(message);
                __android_log_print(ANDROID_LOG_ERROR, "cclover_test", "messages_->idx_ = %zu, current = %zu", messages_->idx_, i);
            }
        }

        MessageAllocator *messages_;

        // Statistic
        static std::atomic<size_t> g_queue_extra_stack_meta_allocated;
        static std::atomic<size_t> g_queue_extra_stack_meta_kept;
        static message_queue_counter_t g_message_queue_counter_;
        static message_queue_counter_t g_allocation_queue_counter_;

    private:

    };

    class BufferQueueContainer {
    public:
        BufferQueueContainer() : queue_(nullptr) {}

        ~BufferQueueContainer() {
            delete queue_;
        }

        BufferQueue *queue_;

        std::mutex mutex_;

        static std::atomic<size_t> g_locker_collision_counter;

        inline void lock() {
            if (UNLIKELY(!mutex_.try_lock())) {
                g_locker_collision_counter.fetch_add(1, std::memory_order_relaxed);
                mutex_.lock();
            }

            if (UNLIKELY(!queue_)) {
                queue_ = new BufferQueue(SIZE_AUGMENT);
            }
        }

        inline void unlock() {
            mutex_.unlock();
        }
    };

    class BufferManagement {
    public:

        void enqueue_message(uintptr_t key, std::function<void()> runnable);

        BufferManagement();

        ~BufferManagement();

        void start_process();

    private:

        std::vector<BufferQueueContainer *> containers_;

        [[noreturn]] static void process_routine(BufferManagement *this_);

        BufferQueue *queue_swapped_ = nullptr;

        bool processing_ = false;
        pthread_t thread_{};
    };

}

#endif //LIBMATRIX_HOOK_MEMORY_BUFFER_QUEUE_H
