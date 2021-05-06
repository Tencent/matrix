#ifndef BENCHMARK_LOG_H
#define BENCHMARK_LOG_H

#include <time.h>
#include <android/log.h>
#include <cstdio>

#define BENCHMARK_LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args);
#define BENCHMARK_LOGI(TAG, FMT, args...) __android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args);
#define BENCHMARK_LOGE(TAG, FMT, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args);


#endif //BENCHMARK_LOG_H
