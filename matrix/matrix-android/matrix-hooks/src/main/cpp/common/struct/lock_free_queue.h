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

#ifndef lock_free_queue_h
#define lock_free_queue_h

#include <cstdint>

namespace matrix {

    template<class T>
    struct QueueNode {
        T t_;
        std::atomic<QueueNode<T>*> next_;
    };

    template<typename T>
    class LockFreeQueue {
    public:
        LockFreeQueue() {
            keep_stage_ = (QueueNode<T> *) calloc(1, sizeof(QueueNode<T>));
            stage_.store(keep_stage_);
            head_.store(keep_stage_);
            tail_.store(keep_stage_);
        }

        ~LockFreeQueue() {
            free(keep_stage_);
        }

        bool offer(QueueNode<T> *node) {
            if (UNLIKELY(!node)) return false;
            if (UNLIKELY(node->next_)) return false;

            for (;;) {
                auto tail_node = tail_.load(std::memory_order_acquire);
                auto next_node = tail_node->next_.load(std::memory_order_acquire);

                auto tail_node_2 = tail_.load(std::memory_order_acquire);
                if (LIKELY(tail_node == tail_node_2)) {
                    if (next_node == nullptr) {
                        if (tail_node->next_.compare_exchange_weak(next_node, node)) {
                            tail_.compare_exchange_strong(tail_node, node);
                            return true;
                        }
                    }
                    else {
                        tail_.compare_exchange_strong(tail_node, next_node);
                    }
                }
            }
        }

        QueueNode<T> * poll() {

            for (;;) {
                auto head_node = head_.load(std::memory_order_acquire);

                auto tail_node = tail_.load(std::memory_order_acquire);
                auto next_node = head_node->next_.load(std::memory_order_acquire);

                auto head_node_2 = head_.load(std::memory_order_acquire);
                if (LIKELY(head_node == head_node_2)) {
                    if (head_node == tail_node) {
                        if (next_node == nullptr)
                            return nullptr;

                        tail_.compare_exchange_strong(tail_node, next_node);

                    } else {
                        if (next_node == nullptr)
                            continue;
                        if (head_.compare_exchange_weak(head_node, next_node)) {
                            return next_node;
                        }
                    }
                }
            }
        }

        QueueNode<T> * do_stage(QueueNode<T> * node) {

            if (UNLIKELY(!node)) return node;

            for (;;) {
                auto stage_node = stage_.load(std::memory_order_acquire);
                auto head_node = head_.load(std::memory_order_acquire);
                if (stage_node == head_node) {
                    return node;
                } else if (stage_.compare_exchange_strong(stage_node, node)) {
                    return stage_node;
                }
            }
        }

    private:

        std::atomic<QueueNode<T>*> head_;
        std::atomic<QueueNode<T>*> tail_;

        std::atomic<QueueNode<T>*> stage_;
        QueueNode<T>* keep_stage_;
    };
}
#endif /* lock_free_queue_h */
