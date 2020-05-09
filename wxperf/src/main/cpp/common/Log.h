//
// Created by Yves on 2019-08-08.
//

#ifndef MEMINFO_LOG_H
#define MEMINFO_LOG_H

#include <android/log.h>

#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) __android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#endif //MEMINFO_LOG_H
