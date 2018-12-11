/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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
// Created by liyongjie on 2017/12/7.
//

#ifndef MATRIX_IO_CANARY_IO_CANARY_ENV_H
#define MATRIX_IO_CANARY_IO_CANARY_ENV_H

namespace iocanary {

    enum IOCanaryConfigKey {
        kMainThreadThreshold = 0,
        kSmallBufferThreshold,
        kRepeatReadThreshold,

        //!!kConfigKeysLen always the last one!!
        kConfigKeysLen
    };

    class IOCanaryEnv {
    public:
        IOCanaryEnv();
        void SetConfig(IOCanaryConfigKey key, long val);

        long GetJavaMainThreadID() const;
        long GetMainThreadThreshold() const;
        long GetSmallBufferThreshold() const;
        long GetRepeatReadThreshold() const;

        //in μs.
        //it may be negative if the io-cost more than POSSIBLE_NEGATIVE_THRESHOLD
        //else it can be negligible
        //80% of the well-known 16ms
        constexpr static const int kPossibleNegativeThreshold = 13*1000;
        constexpr static const int kSmallBufferOpTimesThreshold = 20;
    private:
        long GetConfig(IOCanaryConfigKey key) const;

        //in μs
        constexpr static const int kDefaultMainThreadTriggerThreshold = 500*1000;
        //We take 4096B(4KB) as a small size of the buffer
        constexpr static const int kDefaultBufferSmallThreshold = 4096;
        constexpr static const int kDefaultRepeatReadThreshold = 5;

        long configs_[IOCanaryConfigKey::kConfigKeysLen];
    };
}

#endif //MATRIX_IO_CANARY_IO_CANARY_ENV_H
