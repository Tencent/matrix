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
// Author: liyongjie
// Created by liyongjie
//

#ifndef MATRIX_IO_CANARY_IO_CANARY_UTILS_H
#define MATRIX_IO_CANARY_IO_CANARY_UTILS_H

#include <stdlib.h>

#include <string>
#include <vector>

namespace iocanary {
    int64_t GetSysTimeMicros();
    int64_t GetSysTimeMilliSecond();

    int64_t GetTickCountMicros();
    int64_t GetTickCount();

    intmax_t GetMainThreadId();
    intmax_t GetCurrentThreadId();
    bool IsMainThread();

    std::string GetLatestStack(const std::string& stack, int count);
    void Split(const std::string &src_str, std::vector<std::string> &sv, const char delim, int cnt = 0);
    int GetFileSize(const char* file_path);
    std::string MD5(std::string);
}


#endif //MATRIX_IO_CANARY_IO_CANARY_UTILS_H
