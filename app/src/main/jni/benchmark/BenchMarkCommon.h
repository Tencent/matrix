//
// Created by Yves on 2020-05-12.
//

#ifndef LIBWXPERF_JNI_BENCHMARKCOMMON_H
#define LIBWXPERF_JNI_BENCHMARKCOMMON_H

#include <time.h>
#include <android/log.h>

#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) __android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#define NanoSeconds_Start(timestamp) \
        struct timespec timestamp; \
        if (clock_gettime(CLOCK_REALTIME, &timestamp)) { \
            LOGE(MEMORY_BENCHMARK_TAG, "Err: Get time failed."); \
        }

#define NanoSeconds_End(tag, timestamp) \
        { \
            struct timespec tms_end; \
            if (clock_gettime(CLOCK_REALTIME, &tms_end)) { \
                LOGE(MEMORY_BENCHMARK_TAG, "Err: Get time failed."); \
            } \
            LOGE(MEMORY_BENCHMARK_TAG, #tag" %lu - %lu = costs: %luns", \
                     tms_end.tv_sec * 1000 * 1000 * 1000 + tms_end.tv_nsec, \
                     timestamp.tv_sec * 1000 * 1000 * 1000 + timestamp.tv_nsec, \
                     (tms_end.tv_sec * 1000 * 1000 * 1000 + tms_end.tv_nsec - (timestamp.tv_sec * 1000 * 1000 * 1000 + timestamp.tv_nsec))); \
        }


#endif //LIBWXPERF_JNI_BENCHMARKCOMMON_H
