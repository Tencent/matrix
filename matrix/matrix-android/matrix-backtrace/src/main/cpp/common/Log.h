//
// Created by Yves on 2019-08-08.
//

#ifndef MEMINFO_LOG_H
#define MEMINFO_LOG_H

#include <time.h>
#include <android/log.h>
#include <cstdio>

namespace wechat_backtrace {

    typedef int (*internal_logger_func)(int log_level, const char *tag, const char *format,
                                        va_list varargs);

    extern "C" void internal_init_logger(internal_logger_func logger_func);
    extern "C" void internal_logger(int log_level, const char *tag, const char *format, ...);
    extern "C" void
    internal_vlogger(int log_level, const char *tag, const char *format, va_list varargs);

}

#ifdef EnableLOG
#define LOGD(TAG, FMT, args...) wechat_backtrace::internal_logger(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) wechat_backtrace::internal_logger(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) wechat_backtrace::internal_logger(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#define STACK_CHECK_START(S) \
    const int barrier_size = S; \
    char barrier_stub = (char)barrier_size;    \
    char stack_barrier[barrier_size]; \
    memset(stack_barrier, barrier_stub, barrier_size);

#define STACK_CHECK_END \
    for (char i : stack_barrier) { \
        if (i != barrier_stub) { \
            QUT_LOG("Check stack Failed! index(%llu)", (unsigned long long) barrier_size); \
            abort(); \
        }\
    }

#define DWARF_CFA_LOG(FMT, args...) //wechat_backtrace::internal_logger(ANDROID_LOG_ERROR, "Matrix.Backtrace.DWARF-CFA", FMT, ##args)
#define DWARF_OP_LOG(FMT, args...) wechat_backtrace::internal_logger(ANDROID_LOG_ERROR, "Matrix.Backtrace.DWARF-OP", FMT, ##args)
#define QUT_TMP_LOG(FMT, args...) //wechat_backtrace::internal_logger(ANDROID_LOG_ERROR, "Matrix.Backtrace.TMP", FMT, ##args)
#define QUT_STAT_LOG(FMT, args...) wechat_backtrace::internal_logger(ANDROID_LOG_ERROR, "Matrix.Backtrace.STAT", FMT, ##args)
#define QUT_DEBUG_LOG(FMT, args...) wechat_backtrace::internal_logger(ANDROID_LOG_ERROR, "Matrix.Backtrace.DEBUG", FMT, ##args)
#define QUT_LOG(FMT, args...) wechat_backtrace::internal_logger(ANDROID_LOG_ERROR, "Matrix.Backtrace.Native", FMT, ##args)
#else
#define LOGD(TAG, FMT, args...)
#define LOGI(TAG, FMT, args...)
#define LOGE(TAG, FMT, args...)

#define STACK_CHECK_START(S)
#define STACK_CHECK_END

#define DWARF_CFA_LOG(FMT, args...)
#define DWARF_OP_LOG(FMT, args...)
#define QUT_STAT_LOG(FMT, args...)
#define QUT_TMP_LOG(FMT, args...)
#define QUT_DEBUG_LOG(FMT, args...)
#define QUT_LOG(FMT, args...)
#endif


#define __FAKE_USE_VA_ARGS(...) ((void)(0))
#define __android_second(dummy, second, ...) second
#define __android_rest(first, ...) , ##__VA_ARGS__

#define android_printAssert(cond, tag, ...)                     \
  __android_log_assert(cond, tag,                               \
                       __android_second(0, ##__VA_ARGS__, NULL) \
                           __android_rest(__VA_ARGS__))

#ifndef LOG_ALWAYS_FATAL_IF
#define LOG_ALWAYS_FATAL_IF(cond, LOG_TAG, ...)                                                    \
  ((__predict_false(cond)) ? (__FAKE_USE_VA_ARGS(__VA_ARGS__),                            \
                              ((void)android_printAssert(#cond, LOG_TAG, ##__VA_ARGS__))) \
                           : ((void)0))
#endif

#ifndef LOG_ALWAYS_FATAL
#define LOG_ALWAYS_FATAL(LOG_TAG, ...) \
  (((void)android_printAssert(NULL, LOG_TAG, ##__VA_ARGS__)))
#endif

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"

#define BYTE_TO_BINARY(byte)  \
      (byte & 0x80 ? '1' : '0'), \
      (byte & 0x40 ? '1' : '0'), \
      (byte & 0x20 ? '1' : '0'), \
      (byte & 0x10 ? '1' : '0'), \
      (byte & 0x08 ? '1' : '0'), \
      (byte & 0x04 ? '1' : '0'), \
      (byte & 0x02 ? '1' : '0'), \
      (byte & 0x01 ? '1' : '0')

extern "C" int flogger(FILE *fp, const char *fmt, ...);


#endif //MEMINFO_LOG_H
