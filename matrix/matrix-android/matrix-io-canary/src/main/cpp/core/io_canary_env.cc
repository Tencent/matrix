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

#include "io_canary_env.h"

namespace iocanary {

    IOCanaryEnv::IOCanaryEnv() {
        configs_[IOCanaryConfigKey::kMainThreadThreshold] = kDefaultMainThreadTriggerThreshold;
        configs_[IOCanaryConfigKey::kSmallBufferThreshold] = kDefaultBufferSmallThreshold;
        configs_[IOCanaryConfigKey::kRepeatReadThreshold] = kDefaultRepeatReadThreshold;
    }

    void IOCanaryEnv::SetConfig(IOCanaryConfigKey key, long val) {
        if (key >= IOCanaryConfigKey::kConfigKeysLen) {
            return;
        }

        configs_[key] = val;
    }

    long IOCanaryEnv::GetMainThreadThreshold() const {
        return GetConfig(IOCanaryConfigKey::kMainThreadThreshold);
    }

    long IOCanaryEnv::GetSmallBufferThreshold() const {
        return GetConfig(IOCanaryConfigKey::kSmallBufferThreshold);
    }

    long IOCanaryEnv::GetRepeatReadThreshold() const {
        return GetConfig(IOCanaryConfigKey::kRepeatReadThreshold);
    }

    long IOCanaryEnv::GetConfig(IOCanaryConfigKey key) const {
        if (key >= IOCanaryConfigKey::kConfigKeysLen) {
            return -1;
        }

        return configs_[key];
    }
}
