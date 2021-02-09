//
// Created by Yves on 2020/12/28.
//
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include "Log.h"
#include "Predefined.h"

extern "C" BACKTRACE_EXPORT int flogger(FILE *fp, const char *fmt, ...) {
    if (!fp) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(fp, fmt, args);
    va_end(args);
    return ret;
}

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