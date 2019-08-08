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
// File IO operation may block the main thread and bring some bad UEs.
// This detector is to detect whether File IO operation occurs in main thread.
//
// Created by liyongjie on 2017/12/7.
//

#include "main_thread_detector.h"
#include "comm/io_canary_utils.h"

namespace iocanary {

    void FileIOMainThreadDetector::Detect(const IOCanaryEnv &env, const IOInfo &file_io_info,
                                          std::vector<Issue>& issues) {

        //__android_log_print(ANDROID_LOG_ERROR, "FileIOMainThreadDetector", "Detect  main-thread-id：%d, thread-id:%d max_continual_rw_cost_time_μs_:%d threshold:%d"
          //      , env.GetJavaMainThreadID(), file_io_info.java_context_.thread_id_, file_io_info.max_continual_rw_cost_time_μs_, env.GetMainThreadThreshold());

        if (GetMainThreadId() == file_io_info.java_context_.thread_id_) {
            int type = 0;
            if (file_io_info.max_once_rw_cost_time_μs_ > IOCanaryEnv::kPossibleNegativeThreshold) {
                type = 1;
            }
            if(file_io_info.max_continual_rw_cost_time_μs_ > env.GetMainThreadThreshold()) {
                type |= 2;
            }

            if (type != 0) {
                Issue issue(kType, file_io_info);
                issue.repeat_read_cnt_ = type;  //use repeat to record type
                PublishIssue(issue, issues);
            }
        }
    }
}
