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
// If a process will read the same file many times, it's better to use a memory keep to enhance.
// This detector is to find such a issue for improving.
//
// Created by liyongjie on 2017/12/7.
//

#ifndef MATRIX_IO_CANARY_IO_REPEAT_READ_DETECTOR_H
#define MATRIX_IO_CANARY_IO_REPEAT_READ_DETECTOR_H

#include <unordered_map>
#include "detector.h"

namespace iocanary {
    class RepeatReadInfo {
    public:
        RepeatReadInfo(const std::string& path, const std::string& java_stack, long java_thread_id
                , long op_size, int file_size);

        bool operator== (const RepeatReadInfo& target) const;

        void IncRepeatReadCount();
        int GetRepeatReadCount();
        std::string GetStack();

    public:
        const std::string path_;
        const std::string java_stack_;
        const long java_thread_id_;
        const long op_size_;
        const int file_size_;

        int repeat_cnt_;
        int64_t op_timems;
    };

    class FileIORepeatReadDetector : public FileIODetector {
    public:
        virtual void Detect(const IOCanaryEnv& env, const IOInfo& file_io_info, std::vector<Issue>& issues) override ;

        constexpr static const IssueType kType = IssueType::kIssueRepeatRead;
    private:
        std::unordered_map<std::string, std::vector<RepeatReadInfo>> observing_map_;
    };
}
#endif //MATRIX_IO_CANARY_IO_REPEAT_READ_DETECTOR_H
