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

#ifndef __SQLITELINT_LOG_H__
#define __SQLITELINT_LOG_H__
#include <stdarg.h>
namespace sqlitelint {
	//如果上层log level定义与此不一致，需要上层进行转换。
	typedef enum {
		kLevelVerbose = 2,
		kLevelDebug,    // Detailed information on the flow through the system.
		kLevelInfo,     // Interesting runtime events (startup/shutdown), should be conservative and keep to a minimum.
		kLevelWarn,     // Other runtime situations that are undesirable or unexpected, but not necessarily "wrong".
		kLevelError,    // Other runtime errors or unexpected conditions.
		kLevelFatal,    // Severe errors that cause premature termination.
		kLevelNone,     // Special level used to disable all log messages.
	} SLogLevel;

	extern SLogLevel kLogLevel;//below this level will not print log

#define sVerbose(fmt,...)     SLog(kLevelVerbose, (fmt), ##__VA_ARGS__)
#define sDebug(fmt,...)     SLog(kLevelDebug, (fmt), ##__VA_ARGS__)
#define sInfo(fmt,...)     SLog(kLevelInfo, (fmt), ##__VA_ARGS__)
#define sWarn(fmt,...)     SLog(kLevelWarn, (fmt), ##__VA_ARGS__)
#define sError(fmt,...)     SLog(kLevelError, (fmt), ##__VA_ARGS__)
#define sFatal(fmt,...)     SLog(kLevelFatal, (fmt), ##__VA_ARGS__)


	typedef int(*SLogFunc)(int prio, const char *msg);
	void SetSLogFunc(SLogFunc func);
	void SetSLogLevel(SLogLevel level);

	int SLog(int prio, const char *fmt, ...)
#ifdef __GNUC__
		__attribute__((format(printf, 2, 3)))
#endif
		;
}
#endif
