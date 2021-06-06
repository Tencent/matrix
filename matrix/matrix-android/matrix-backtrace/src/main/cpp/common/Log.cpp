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

#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include "Log.h"
#include "Predefined.h"

namespace wechat_backtrace {

    static volatile internal_logger_func _logger_func = nullptr;

    extern "C" BACKTRACE_EXPORT void internal_init_logger(internal_logger_func logger_func) {
        _logger_func = logger_func;
    }

    extern "C" BACKTRACE_EXPORT void
    internal_logger(int log_level, const char *tag, const char *format, ...) {
        va_list ap;
        va_start(ap, format);
        internal_vlogger(log_level, tag, format, ap);
        va_end(ap);
    }

    extern "C" BACKTRACE_EXPORT void
    internal_vlogger(int log_level, const char *tag, const char *format, va_list varargs) {
        internal_logger_func tmp_logger_func = _logger_func;
        if (tmp_logger_func) {
            tmp_logger_func(log_level, tag, format, varargs);
        }
    }

}