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
// Created by Yves on 2020/7/9.
//
#include <pthread.h>
#include <mutex>
#include "ReentrantPrevention.h"
#include "Log.h"

#define TAG "ReentrantPrevention"

pthread_key_t m_rp_key;
//std::mutex m_rp_mutex;

void rp_init() {
    if (!m_rp_key) {
        pthread_key_create(&m_rp_key, nullptr);
    }
}

bool rp_acquire() {
    auto counter = static_cast<size_t *>(pthread_getspecific(m_rp_key));
    if (!counter) {
        counter = new size_t;
        *counter = 0;
        pthread_setspecific(m_rp_key, counter);
    }

    if (*counter) {
        return false;
    }

    (*counter)++;
    return true;
}

void rp_release() {
    auto counter = static_cast<size_t *>(pthread_getspecific(m_rp_key));
    if (!counter) {
        LOG_ALWAYS_FATAL(TAG, "calling rp_release() before rp_acquire");
    }

    (*counter)--;
}

#undef TAG