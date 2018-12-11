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

#include "logger.h"
#include <stdio.h>

namespace sqlitelint {
	static SLogFunc kLogfunc;
    SLogLevel kLogLevel = kLevelVerbose;

	static int dummyLog(int prio, const char *msg){
		return 0;
	}


	void SetSLogFunc(SLogFunc func){
		if (!func) func = dummyLog;
		kLogfunc = func;
	}

	void SetSLogLevel(SLogLevel level){
		kLogLevel = level;
	}


	int SLog(int prio, const char *fmt, ...){
		if (prio < kLogLevel){
			return -1;
		}
		if (prio < kLogLevel){
			prio = kLogLevel;
		}
		va_list ap;
		char buf[1024];

		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		if (!kLogfunc){
			return dummyLog(prio, buf);
		}

		return kLogfunc(prio, buf);
	}
}
