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

#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <mutex>
#include <jni.h>
#include "Log.h"
#include "Predefined.h"

namespace wechat_backtrace {

    static internal_logger_func logger_func_ = nullptr;
    static bool enable_logger_ = false;
    /* static char *xlog_so_path_ = nullptr; */

    DEFINE_STATIC_LOCAL(std::mutex, lock_,);

    extern "C" BACKTRACE_EXPORT void enable_backtrace_logger(bool enable) {
        enable_logger_ = enable;
    }

    /*
    extern "C" BACKTRACE_EXPORT void set_xlog_logger_path(const char *xlog_so_path, const size_t size) {
        std::lock_guard<std::mutex> lock_guard(lock_);
        if (xlog_so_path_) {
            free(xlog_so_path_);
            xlog_so_path_ = nullptr;
        }

        if (xlog_so_path != nullptr) {
            xlog_so_path_ = static_cast<char *>(malloc(size + 1));
            strncpy(xlog_so_path_, xlog_so_path, size);
            xlog_so_path_[size] = '\0';
        }
    }

    extern "C" BACKTRACE_EXPORT char *get_xlog_logger_path() {
        std::lock_guard<std::mutex> lock_guard(lock_);
        return xlog_so_path_;
    }
    */

    extern "C" BACKTRACE_EXPORT void internal_init_logger(internal_logger_func logger_func) {
        logger_func_ = logger_func;
    }

    extern "C" BACKTRACE_EXPORT internal_logger_func logger_func() {
        return logger_func_;
    }

    extern "C" BACKTRACE_EXPORT void
    internal_logger(int log_level, const char *tag, const char *format, ...) {
        if (!enable_logger_) {
            return;
        }
        va_list ap;
        va_start(ap, format);
        internal_vlogger(log_level, tag, format, ap);
        va_end(ap);
    }

    extern "C" BACKTRACE_EXPORT void
    internal_vlogger(int log_level, const char *tag, const char *format, va_list varargs) {
        if (!enable_logger_) {
            return;
        }
        internal_logger_func tmp_logger_func = logger_func_;
        if (tmp_logger_func) {
            tmp_logger_func(log_level, tag, format, varargs);
        }
    }

}