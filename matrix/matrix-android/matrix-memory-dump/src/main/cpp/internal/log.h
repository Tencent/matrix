//
// Created by M.D. on 2021/10/26.
//

#ifndef MATRIX_ANDROID_LOG_H
#define MATRIX_ANDROID_LOG_H

#if defined(__aarch64__) || defined(__arm__)

#include <Log.h>

#define _info_log(tag, fmt, args...) LOGI(tag, FMT, ##args)
#define _debug_log(tag, fmt, args...) LOGD(tag, FMT, ##args)
#define _error_log(tag, fmt, args...) LOGE(tag, FMT, ##args)

#else

#include <android/log.h>

#define _info_log(tag, fmt, args...) __android_log_print(ANDROID_LOG_INFO, tag, fmt, ##args)
#define _debug_log(tag, fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, tag, fmt, ##args)
#define _error_log(tag, fmt, args...) __android_log_print(ANDROID_LOG_ERROR, tag, fmt, ##args)

#endif

#endif
