#include <cstdio>
#include "log.h"

#if defined(__arm__) || defined(__aarch64__)

typedef int (*logger_t)(int log_level, const char *tag, const char *format, va_list varargs);

extern "C" logger_t logger_func();

void print_log(int level, const char *tag, const char *format, ...) {
    logger_t logger = logger_func();
    if (logger == nullptr) return;
    va_list args;
    va_start(args, format);
    logger(level, tag, format, args);
    va_end(args);
}

#else

void print_log(int level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    __android_log_vprint(level, tag, format, args);
    va_end(args);
}

#endif