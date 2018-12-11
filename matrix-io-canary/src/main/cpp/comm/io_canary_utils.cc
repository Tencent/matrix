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

#include "io_canary_utils.h"
#include "md5.h"
#include <unistd.h>
#include <vector>
#include <sstream>
#include <sys/stat.h>
#include <sys/time.h>
#include <regex>

namespace iocanary {

    // 获取系统的当前时间，单位微秒(us)
    int64_t GetSysTimeMicros() {
#ifdef _WIN32
        // 从1601年1月1日0:0:0:000到1970年1月1日0:0:0:000的时间(单位100ns)
#define EPOCHFILETIME   (116444736000000000UL)
		FILETIME ft;
		LARGE_INTEGER li;
		int64_t tt = 0;
		GetSystemTimeAsFileTime(&ft);
		li.LowPart = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		// 从1970年1月1日0:0:0:000到现在的微秒数(UTC时间)
		tt = (li.QuadPart - EPOCHFILETIME) / 10;
		return tt;
#else
        timeval tv;
        gettimeofday(&tv, 0);
        return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
#endif // _WIN32
    }

    // 获取系统的当前时间，单位毫秒(ms)
    int64_t GetSysTimeMilliSecond() {
        return (int64_t)(GetSysTimeMicros() / 1000);
    }


    int64_t GetTickCountMicros() {
        struct timespec ts;
        int result = clock_gettime(CLOCK_BOOTTIME, &ts);

        if (result != 0) {
            // XXX: there was an error, probably because the driver didn't
            // exist ... this should return
            // a real error, like an exception!
            return 0;
        }
        return (int64_t)ts.tv_sec*1000000 + (int64_t)ts.tv_nsec/1000;
    }

    int64_t GetTickCount() {

        return GetTickCountMicros() / 1000;
    }

    intmax_t GetMainThreadId() {
        static intmax_t pid = getpid();
        return pid;
    }

    intmax_t GetCurrentThreadId() {
        return gettid();
    }

    bool IsMainThread() {
        return GetMainThreadId() == GetCurrentThreadId();
    }


    std::string GetLatestStack(const std::string& stack, int count) {
        std::vector<std::string> lines;
        Split(stack, lines, '\n', count);

        std::regex re("^(.+)(\\(.+\\))$");
        std::string latest_stack;
        for (int i = 0; i < std::min(count, (int)lines.size()); i++) {
            std::cmatch cm;
            if (std::regex_match(lines[i].c_str(), cm, re) && 3 == cm.size()) {
                latest_stack = latest_stack + cm[1].str() + "\n";
            } else {
                latest_stack = latest_stack + lines[i] + "\n";
            }
        }

        return latest_stack;
    }

    void Split(const std::string &src_str, std::vector<std::string> &sv, const char delim, int cnt) {
        sv.clear();
        std::istringstream iss(src_str);
        std::string temp;

        while (std::getline(iss, temp, delim)) {
            sv.push_back(temp);
            if (cnt > 0 && sv.size() >= cnt) {
                break;
            }
        }

    }

    int GetFileSize(const char* file_path) {
        struct stat stat_buf;
        if (-1 == stat(file_path, &stat_buf)) {
            return -1;
        }
        return stat_buf.st_size;
    }
    std::string MD5(std::string str) {
        unsigned char sig[16] = {0};
        MD5_buffer(str.c_str(), str.length(), sig);
        char des[33] = {0};
        MD5_sig_to_string((const char*)sig, des);
        return std::string(des);
    }
}
