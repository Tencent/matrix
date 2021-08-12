//
// Created by YinSheng Tang on 2021/7/21.
//

#ifndef MATRIX_ANDROID_SD_LOG_H
#define MATRIX_ANDROID_SD_LOG_H


#include <stdbool.h>
#include <android/log.h>

extern bool g_semi_dlfcn_log_enabled;
extern int g_semi_dlfcn_log_level;

#define LOGV(tag, fmt, args...) \
    do { \
        if (g_semi_dlfcn_log_enabled && (g_semi_dlfcn_log_level <= ANDROID_LOG_VERBOSE)) { \
            __android_log_print(ANDROID_LOG_VERBOSE, tag, fmt, ##args); \
        } \
    } while (false)

#define LOGD(tag, fmt, args...) \
    do { \
        if (g_semi_dlfcn_log_enabled && (g_semi_dlfcn_log_level <= ANDROID_LOG_DEBUG)) { \
            __android_log_print(ANDROID_LOG_DEBUG, tag, fmt, ##args); \
        } \
    } while (false)

#define LOGI(tag, fmt, args...) \
    do { \
        if (g_semi_dlfcn_log_enabled && (g_semi_dlfcn_log_level <= ANDROID_LOG_INFO)) { \
            __android_log_print(ANDROID_LOG_INFO, tag, fmt, ##args); \
        } \
    } while (false)

#define LOGW(tag, fmt, args...) \
    do { \
        if (g_semi_dlfcn_log_enabled && (g_semi_dlfcn_log_level <= ANDROID_LOG_WARN)) { \
            __android_log_print(ANDROID_LOG_WARN, tag, fmt, ##args); \
        } \
    } while (false)

#define LOGE(tag, fmt, args...) \
    do { \
        if (g_semi_dlfcn_log_enabled && (g_semi_dlfcn_log_level <= ANDROID_LOG_ERROR)) { \
            __android_log_print(ANDROID_LOG_ERROR, tag, fmt, ##args); \
        } \
    } while (false)


#endif //MATRIX_ANDROID_SD_LOG_H
