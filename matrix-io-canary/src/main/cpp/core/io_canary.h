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

#ifndef MATRIX_IO_CANARY_IO_CANARY_H
#define MATRIX_IO_CANARY_IO_CANARY_H

#include <condition_variable>
#include <memory>
#include <deque>
#include "io_info_collector.h"
#include "detector/detector.h"

namespace iocanary {

    typedef void(*OnPublishIssueCallback) (const std::vector<Issue>& published_issues);

    class IOCanary {
    public:
        IOCanary(const IOCanary&) = delete;
        IOCanary& operator=(IOCanary const&) = delete;

        static IOCanary& Get();

        void RegisterDetector(DetectorType type);
        void SetConfig(IOCanaryConfigKey key, long val);
        void SetJavaMainThreadId(long main_thread_id);

        void SetIssuedCallback(OnPublishIssueCallback issued_callback);

        void OnOpen(const char *pathname, int flags, mode_t mode, int open_ret, const JavaContext& java_context);
        void OnRead(int fd, const void *buf, size_t size, ssize_t read_ret, long read_cost);
        void OnWrite(int fd, const void *buf, size_t size, ssize_t write_ret, long write_cost);
        void OnClose(int fd, int close_ret);

    private:
        IOCanary();
        ~IOCanary();

        void OfferFileIOInfo(std::shared_ptr<IOInfo> file_io_info);
        int TakeFileIOInfo(std::shared_ptr<IOInfo>& file_io_info);
        void Detect();

        bool exit_;

        IOCanaryEnv env_;
        OnPublishIssueCallback issued_callback_;
        std::vector<FileIODetector*> detectors_;

        IOInfoCollector collector_;
        std::deque<std::shared_ptr<IOInfo>> queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;
    };

};


#endif //MATRIX_IO_CANARY_IO_CANARY_H
