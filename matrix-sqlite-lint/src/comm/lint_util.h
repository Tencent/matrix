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

#ifndef SQLiteLint_Util_H
#define SQLiteLint_Util_H

#include <algorithm>
#include <string>
#include <sstream>

namespace sqlitelint {
#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <sys/time.h>
#endif  // _WIND32

#if defined(_WIN32) && !defined(CYGWIN)
	typedef __int64 int64_t;
#else
	typedef long long int64t;
#endif  // _WIN32

	bool iequals(const std::string &a, const std::string &b);
	int64_t GetSysTimeMicros();
    void ToLowerCase(std::string &target);
    void ToUpperCase(std::string &target);
    int CompareIgnoreCase(std::string a, std::string b);
	int64_t GetSysTimeMillisecond();
    std::string FormatTime(time_t t);
    template <typename T>
    std::string to_string(T value){
        std::ostringstream os;
        os << value ;
        return os.str() ;
    }

    void trim(std::string &s);
    std::string MD5(std::string);
    bool IsInMainThread();
}
#endif //end SQLiteLint_Util_H
