/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
// Created by Yves on 2021/4/27.
//

#ifndef MATRIX_HOOK_LOG_H
#define MATRIX_HOOK_LOG_H

#include <stdio.h>
#include <android/log.h>
#include "Macros.h"

typedef int (*internal_logger_func)(int log_level, const char *tag, const char *format,
                                    va_list varargs);

EXPORT_C void enable_hook_logger(bool enable);
EXPORT_C void internal_hook_logger(int log_level, const char *tag, const char *format, ...);
EXPORT_C void
internal_hook_vlogger(int log_level, const char *tag, const char *format, va_list varargs);

#ifdef EnableLOG

#undef LOGD
#undef LOGI
#undef LOGE

#define LOGD(TAG, FMT, args...) internal_hook_logger(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) internal_hook_logger(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) internal_hook_logger(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#else
#define LOGD(TAG, FMT, args...)
#define LOGI(TAG, FMT, args...)
#define LOGE(TAG, FMT, args...)

#endif

#ifndef LOG_ALWAYS_FATAL

#define __FAKE_USE_VA_ARGS(...) ((void)(0))
#define __android_second(dummy, second, ...) second
#define __android_rest(first, ...) , ##__VA_ARGS__

#define android_printAssert(cond, tag, ...)                     \
  __android_log_assert(cond, tag,                               \
                       __android_second(0, ##__VA_ARGS__, NULL) \
                           __android_rest(__VA_ARGS__))

#define LOG_ALWAYS_FATAL(LOG_TAG, ...) \
  (((void)android_printAssert(NULL, LOG_TAG, ##__VA_ARGS__)))
#endif

EXPORT_C int flogger0(FILE *fp, const char *fmt, ...);

#endif //MATRIX_HOOK_LOG_H
