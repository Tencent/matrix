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

#include "detector.h"
#include "comm/io_canary_utils.h"

namespace iocanary {

    Issue::Issue(IssueType type, IOInfo file_io_info) : type_(type), key_(GenKey(file_io_info)), file_io_info_(file_io_info) {
        repeat_read_cnt_ = 0;
        stack = file_io_info.java_context_.stack_;
    }

    std::string Issue::GenKey(const IOInfo &file_io_info) {
        return MD5(file_io_info.path_ + ":" + GetLatestStack(file_io_info.java_context_.stack_, 4));
    }

    void FileIODetector::PublishIssue(const Issue &target, std::vector<Issue>& issues) {
        if (IsIssuePublished(target.key_)) {
            return;
        }

        issues.push_back(target);

        MarkIssuePublished(target.key_);
    }

    void FileIODetector::MarkIssuePublished(const std::string &key) {
        published_issue_set_.insert(key);
    }

    bool FileIODetector::IsIssuePublished(const std::string &key) {
        return published_issue_set_.find(key) != published_issue_set_.end();
    }

    FileIODetector::~FileIODetector() {}
}
