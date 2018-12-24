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

#include "comm/lint_util.h"
#include <unistd.h>
#include "md5.h"


/**
 * Created by liyongjie on 16/9/26.
 */

namespace sqlitelint {
	//Case insensitive string comparison
	bool iequals(const std::string& a, const std::string& b) {
		unsigned int sz = a.size();
		if (b.size() != sz)
			return false;
		for (unsigned int i = 0; i < sz; ++i)
		if (tolower(a[i]) != tolower(b[i]))
			return false;
		return true;
	}

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
	int64_t GetSysTimeMillisecond() {
		return (int64_t)(GetSysTimeMicros() / 1000);
	}

    void ToLowerCase(std::string &target) {
        std::transform(target.begin(), target.end(), target.begin(), ::tolower);
    }

    void ToUpperCase(std::string &target) {
        std::transform(target.begin(), target.end(), target.begin(), ::toupper);
    }

    int CompareIgnoreCase(std::string a, std::string b) {
		ToLowerCase(a);
		ToLowerCase(b);
        return a.compare(b);
    }

	std::string FormatTime(time_t t) {
		tm *time = localtime(&t);
		char time_format[26];
		strftime(time_format, 26, "%Y-%m-%d %H:%M:%S", time);
		return time_format;
	}

    void trim(std::string &s){
        if (s.empty()){
            return ;
        }

        s.erase(0,s.find_first_not_of(" "));
        if(s.find_last_not_of(" ") != std::string::npos) {
			s.erase(s.find_last_not_of(" ") + 1);
		} else {
        	s.erase(0);
        }
    }

    std::string MD5(std::string str) {
        unsigned char sig[16] = {0};
        MD5_buffer(str.c_str(), str.length(), sig);
        char des[33] = {0};
        MD5_sig_to_string((const char*)sig, des);
        return std::string(des);
    }

	bool IsInMainThread() {
		static intmax_t pid = getpid();
		return pid == gettid();
	}
}
