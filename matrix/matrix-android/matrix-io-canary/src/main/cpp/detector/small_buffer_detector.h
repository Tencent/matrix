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
// If the buffer of write/read is small, it may not make good use of the capacity of read/write.
// This detector is to find such a issue.
//
// Created by liyongjie on 2017/12/7.
//

#ifndef MATRIX_IO_CANARY_SMALL_BUFFER_DETECTOR_H
#define MATRIX_IO_CANARY_SMALL_BUFFER_DETECTOR_H

#include "detector.h"

namespace iocanary {

    class FileIOSmallBufferDetector : public FileIODetector {
    public:
        virtual void Detect(const IOCanaryEnv& env, const IOInfo& file_io_info, std::vector<Issue>& issues) override ;

        constexpr static const IssueType kType = IssueType::kIssueSmallBuffer;
    };
}
#endif //MATRIX_IO_CANARY_SMALL_BUFFER_DETECTOR_H
