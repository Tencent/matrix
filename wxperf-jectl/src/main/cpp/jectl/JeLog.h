//
// Created by Yves on 2020/7/17.
//

#ifndef LIBWXPERF_JNI_JELOG_H
#define LIBWXPERF_JNI_JELOG_H

#include <android/log.h>

#define LOGD(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#endif //LIBWXPERF_JNI_JELOG_H
