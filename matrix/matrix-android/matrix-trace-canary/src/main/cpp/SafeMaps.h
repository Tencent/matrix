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

// Author: leafjia@tencent.com
//
// SafeMaps.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SAFEMAPS_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SAFEMAPS_H_

#include <sys/types.h>
#include <functional>
#include <cstdint>
#include <string_view>

namespace MatrixTracer {

using ProcessMapsCallback = std::function<
        bool(uintptr_t, uintptr_t, uint16_t, uint64_t, ino_t, std::string_view)>;

bool readProcessMaps(pid_t pid, const ProcessMapsCallback& callback);

bool readMapsFile(const char *fileName, const ProcessMapsCallback& callback);

static bool readSelfMaps(const ProcessMapsCallback& callback) {
    return readMapsFile("/proc/self/maps", callback);
}


class SafeMaps {
 public:
    explicit SafeMaps(pid_t pid) : mPid(pid) {}
    virtual ~SafeMaps() = default;

    virtual bool Parse();

 private:
    pid_t mPid;
};

}   // namespace MatrixTracer

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SAFEMAPS_H_