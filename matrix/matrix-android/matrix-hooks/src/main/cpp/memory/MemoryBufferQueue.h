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
#include <common/Macros.h>
#include <Backtrace.h>

class memory_meta_container;

namespace matrix {

    __attribute__((noinline)) void _hook_check(bool assertion);

    static const unsigned int MAX_PTR_SLOT = 1 << 8;
    static const unsigned int PTR_MASK = MAX_PTR_SLOT - 1;

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
        memory_backtrace_t backtrace{0};
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

    typedef struct {
        // Statistic
        std::atomic<size_t> realloc_memory_counter_;
        std::atomic<size_t> realloc_counter_;
        std::atomic<size_t> realloc_failure_counter_;
        std::atomic<size_t> realloc_reach_limit_counter_;
    } message_queue_counter_t;

    template<typename T>
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
        T *queue_ = nullptr;

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

            void *buffer = realloc(queue_, sizeof(T) * resize);

            if (LIKELY(buffer)) {
                queue_ = static_cast<T *>(buffer);

                counter_->realloc_memory_counter_.fetch_sub(sizeof(T) * size_);
                size_ = resize;
                counter_->realloc_memory_counter_.fetch_add(sizeof(T) * size_);

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
            const size_t limit_size = (MEMORY_OVER_LIMIT / (sizeof(message_t) * 2 + sizeof(allocation_message_t)));

            messages_ = new MessageAllocator<message_t>(
                    size_augment * 2,
                    limit_size * 2 * sizeof(message_t),
                    &g_message_queue_counter_);
            allocations_ = new MessageAllocator<allocation_message_t>(
                    size_augment,
                    limit_size * sizeof(allocation_message_t),
                    &g_allocation_queue_counter_);
            messages_->buffer_realloc(true);
            allocations_->buffer_realloc(true);
        };

        ~BufferQueue() {
            delete messages_;
            delete allocations_;
        };

        inline allocation_message_t *enqueue_allocation_message(bool is_mmap) {

            if (UNLIKELY(!messages_->check_realloc()) ||
                UNLIKELY(!allocations_->check_realloc())) {
                return nullptr;
            }

            message_t *msg_idx = &messages_->queue_[messages_->idx_++];
            msg_idx->type = is_mmap ? message_type_mmap : message_type_allocation;
            msg_idx->index = allocations_->idx_;

            allocation_message_t *buffer = &allocations_->queue_[allocations_->idx_++];
            *buffer = {};
            return buffer;
        }

        inline bool enqueue_deletion_message(uintptr_t ptr, bool is_munmap) {

            // Fast path.
            if (messages_->idx_ > 0) {
                message_t *msg_idx = &messages_->queue_[messages_->idx_ - 1];
                if (((!is_munmap && msg_idx->type == message_type_allocation)
                     || (is_munmap && msg_idx->type == message_type_mmap))
                    && allocations_->queue_[msg_idx->index].ptr == ptr) {
                    msg_idx->type = message_type_nil;
                    messages_->idx_--;
                    allocations_->idx_--;
                    return true;
                }
            }

            if (UNLIKELY(!messages_->check_realloc())) {
                return false;
            }

            message_t *msg_idx = &messages_->queue_[messages_->idx_++];
            msg_idx->type = is_munmap ? message_type_munmap : message_type_deletion;
            msg_idx->ptr = ptr;

            return true;
        }

        inline bool empty() const {
            return messages_->idx_ == 0;
        }

        inline void reset() {
            messages_->reset();
            allocations_->reset();
        }

        inline size_t size() {
            return messages_->idx_;
        }

        inline void process(std::function<void(message_t *, allocation_message_t *)> callback) {
            for (size_t i = 0; i < messages_->idx_; i++) {
                message_t *message = &messages_->queue_[i];
                allocation_message_t *allocation_message = nullptr;
                if (message->type == message_type_allocation ||
                    message->type == message_type_mmap) {
                    allocation_message = &allocations_->queue_[message->index];
                }
                callback(message, allocation_message);
            }
        }

        MessageAllocator<message_t> *messages_;
        MessageAllocator<allocation_message_t> *allocations_;

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
        static std::atomic<size_t> g_message_overflow_counter;

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
        BufferManagement(memory_meta_container *memory_meta_container);

        ~BufferManagement();

        void start_process();

        std::vector<BufferQueueContainer *> containers_;

    private:

        [[noreturn]] static void process_routine(BufferManagement *this_);

        BufferQueue *queue_swapped_ = nullptr;

        memory_meta_container *memory_meta_container_ = nullptr;

        bool processing_ = false;
        pthread_t thread_{};
    };
}

#endif //LIBMATRIX_HOOK_MEMORY_BUFFER_QUEUE_H
