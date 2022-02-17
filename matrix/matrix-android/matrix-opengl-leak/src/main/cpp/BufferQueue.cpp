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
#include <utility>
#include <Log.h>
#include "BufferQueue.h"

namespace matrix {

    std::atomic<size_t> BufferQueueContainer::g_locker_collision_counter = 0;

    message_queue_counter_t BufferQueue::g_message_queue_counter_{};

    BufferManagement::BufferManagement() {
        containers_.reserve(MAX_PTR_SLOT);
        for (int i = 0; i < MAX_PTR_SLOT; ++i) {
            auto container = new BufferQueueContainer();
            containers_.emplace_back(container);
        }
    }

    BufferManagement::~BufferManagement() {

        for (auto container : containers_) {
            delete container;
        }

        delete queue_swapped_;
    }

    [[noreturn]] void BufferManagement::process_routine(BufferManagement *this_) {
        while (true) {
            if (!this_->queue_swapped_) this_->queue_swapped_ = new BufferQueue(SIZE_AUGMENT);

            size_t busy_queue = 0;
            for (auto container : this_->containers_) {
                BufferQueue *swapped = nullptr;
                {
                    std::lock_guard<std::mutex> lock(container->mutex_);
                    if (container->queue_ && !container->queue_->empty()) {
                        swapped = container->queue_;
                        container->queue_ = this_->queue_swapped_;
                    }
                }

                if (swapped && swapped->size() >= 5) {
                    busy_queue++;
                }
                if (swapped) {
                    swapped->process([&](message_t *message) {
                        message->runnable();
                    });
                    swapped->reset();
                    this_->queue_swapped_ = swapped;
                }
            }
            this_->busy_ratio = ((float) busy_queue) / this_->containers_.size();

            if (this_->busy_ratio > 0.9f) { // Super busy
                continue;
            } else if (this_->busy_ratio > 0.6f) { // Busy
                usleep(PROCESS_BUSY_INTERVAL);
            } else if (this_->busy_ratio > 0.3f) {
                usleep(PROCESS_NORMAL_INTERVAL);
            } else if (this_->busy_ratio > 0.1f) {
                usleep(PROCESS_LESS_NORMAL_INTERVAL);
            } else {
                usleep(PROCESS_IDLE_INTERVAL);
            }
        }
    }

    int BufferManagement::get_queue_size() {

        int queue_size = 0;
        for (auto container : containers_) {
            {
                std::lock_guard<std::mutex> lock(container->mutex_);
                if (container->queue_ && !container->queue_->empty()) {
                    queue_size += container->queue_->size();
                }
            }
        }
        return queue_size;
    }

    void BufferManagement::start_process() {
        if (processing_) return;
        processing_ = true;
        pthread_create(&thread_, nullptr,
                       reinterpret_cast<void *(*)(void *)>(&BufferManagement::process_routine),
                       this);
        pthread_detach(thread_);
    }

    void BufferManagement::enqueue_message(uintptr_t key, std::function<void()> runnable) {
        BufferQueueContainer *container = this->containers_[container_hash(key)];
        container->lock();
        container->queue_->enqueue_message(std::move(runnable));
        container->unlock();
    }

}