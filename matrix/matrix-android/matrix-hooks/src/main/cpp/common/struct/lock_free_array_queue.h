/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef lock_free_array_queue_h
#define lock_free_array_queue_h

#include <cstdint>
#include <stdatomic.h>

#include <common/Macros.h>

namespace matrix {

#define CACHE_LINE_SIZE     64

    template<typename T>
    class ObjectStorage {
    public:
        ObjectStorage() = default;
        virtual ~ObjectStorage() = default;
        virtual size_t provide() = 0;
        virtual T *get(size_t idx) = 0;
    };

    template<typename T, size_t Reserved>
    class FixedObjectStorage : public ObjectStorage<T> {
    public:
        FixedObjectStorage() = default;

        virtual ~FixedObjectStorage() = default;

        inline size_t provide() override {
            size_t idx = available_.fetch_add(1);
            if (UNLIKELY(idx >= Reserved)) {
                available_.store(Reserved);
                return SIZE_MAX;
            }
            return idx;
        }

        inline T *get(size_t idx) override {
            return &objects[idx];
        }

        std::atomic<size_t> available_ = 0;

    private:
        T objects[Reserved];
    };

    template<typename T, size_t Reserved>
    class SPMC_FixedFreeList {
    public:
        SPMC_FixedFreeList(): mask_(Reserved - 1), head_(Reserved), tail_(0) {
            for (size_t i = 0; i < Reserved; i++) {
                free_[i] = i;
            }
        };

        virtual ~SPMC_FixedFreeList() = default;

        uint32_t allocate_spmc() {

            size_t idx;
            for (;;) {
                size_t head = head_.load(std::memory_order_acquire);
                size_t tail = tail_.load(std::memory_order_acquire);
                if (UNLIKELY(tail == head)) {
                    return INT_MAX;
                }

                idx = free_[tail & mask_];
                if (tail_.compare_exchange_weak(tail, tail + 1)) {
                    break;
                }
            }
            return idx;
        }

        void deallocate_spmc(uint32_t idx) {

            size_t head = head_.load(std::memory_order_acquire);
            free_[head & mask_] = idx;
            bool ok = head_.compare_exchange_strong(head, head + 1);
            HOOK_CHECK(ok);
            return;
        }

        T * get(size_t idx) {
            return storage_.get(idx);
        }

    private:

        const size_t mask_;

        FixedObjectStorage<T, Reserved> storage_;

        char pad1[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];
        std::atomic<size_t> head_;
        char pad2[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];
        std::atomic<size_t> tail_;
        char pad3[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];

        uint32_t free_[Reserved];
    };

    template<typename T, size_t Reserved>
    class LockFreeArrayQueue {
    public:
        LockFreeArrayQueue() : mask_(Reserved - 1), size_(Reserved), head_first_(0), head_second_(0), tail_first_(0) {
        }

        ~LockFreeArrayQueue() = default;

        bool offer_mpsc(const T & t) {

            uint32_t head, tail, next;

            for (;;) {
                head = head_first_.load(std::memory_order_acquire);
                tail = tail_first_.load(std::memory_order_acquire);
                if (UNLIKELY((head - tail) > mask_)) {
                    return false;
                }
                next = head + 1;
                if (head_first_.compare_exchange_weak(head, next)) {
                    break;
                }
            }

            messages_[head & mask_] = t;

            for (;;) {
                if (head_second_.compare_exchange_weak(head, next)) {
                    break;
                } else {
                    pause();
                }
            }

            return true;
        }

        bool poll_mpsc(T & ret) {

            uint32_t tail, head;
            int ok;

            tail = tail_first_.load(std::memory_order_acquire);
            head = head_second_.load(std::memory_order_acquire);

            if (tail == head)
                return false;

            HOOK_CHECK(!(tail > head && (head - tail) > mask_));

            ret = messages_[tail & mask_];
            ok = tail_first_.compare_exchange_strong(tail, tail + 1);
            HOOK_CHECK(ok)

            return true;
        }

        void pause() {
//            sched_yield();
//            sleep(0);
        };

    private:
        const uint32_t mask_;
        const uint32_t size_;

        char pad0[CACHE_LINE_SIZE - 2 * sizeof(uint32_t)];

        std::atomic<uint32_t> head_first_;
        char pad1[CACHE_LINE_SIZE - sizeof(std::atomic<uint32_t>)];
        std::atomic<uint32_t> head_second_;
        char pad2[CACHE_LINE_SIZE - sizeof(std::atomic<uint32_t>)];
        std::atomic<uint32_t> tail_first_;
        char pad3[CACHE_LINE_SIZE - sizeof(std::atomic<uint32_t>)];

        T messages_[Reserved];
    };



}
#endif /* lock_free_array_queue_h */
