//
// Created by Yves on 2019-08-08.
//

#ifndef MEMINFO_LOG_H
#define MEMINFO_LOG_H

#include <time.h>
#include <android/log.h>
#include <cstdio>

#define LOGD(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

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

#define NanoSeconds_Start(timestamp) \
//        struct timespec timestamp; \
//        if (clock_gettime(CLOCK_REALTIME, &timestamp)) { \
//            LOGE(MEMORY_BENCHMARK_TAG, "Err: Get time failed."); \
//        }

#define NanoSeconds_End(result, timestamp) \
//        long long result = 0; \
//        { \
//            struct timespec tms_end; \
//            if (clock_gettime(CLOCK_REALTIME, &tms_end)) { \
//                LOGE(MEMORY_BENCHMARK_TAG, "Err: Get time failed."); \
//            } \
//            result = (tms_end.tv_sec * 1000 * 1000 * 1000 + tms_end.tv_nsec - (timestamp.tv_sec * 1000 * 1000 * 1000 + timestamp.tv_nsec)); \
//        }

int flogger(FILE* fp , const char* fmt, ...);

#endif //MEMINFO_LOG_H
