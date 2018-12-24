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

#ifndef MATRIX_IO_CANARY_DETECTOR_H
#define MATRIX_IO_CANARY_DETECTOR_H

#include <vector>
#include <set>
#include "core/io_info_collector.h"
#include "core/io_canary_env.h"

namespace iocanary {

    typedef enum {
        kDetectorMainThreadIO = 0,
        kDetectorSmallBuffer,
        kDetectorRepeatRead
    } DetectorType;

    typedef enum {
        kIssueMainThreadIO = 1,
        kIssueSmallBuffer,
        kIssueRepeatRead
    } IssueType ;

    class Issue {
    public:
        Issue(IssueType type, IOInfo file_io_info);

        const IssueType type_;
        const IOInfo file_io_info_;
        const std::string key_;

        int repeat_read_cnt_;
        std::string stack;
    private:
        static std::string GenKey(const IOInfo& file_io_info);
    };

    class FileIODetector {
    public:
        virtual ~FileIODetector();

        virtual void Detect(const IOCanaryEnv& env, const IOInfo& file_io_info, std::vector<Issue>& issues) = 0;

    protected:
        void PublishIssue(const Issue& target, std::vector<Issue>& issues);

    private:
        void MarkIssuePublished(const std::string& key);
        bool IsIssuePublished(const std::string& key);

        std::set<std::string> published_issue_set_;
    };
}
#endif //MATRIX_IO_CANARY_DETECTOR_H
