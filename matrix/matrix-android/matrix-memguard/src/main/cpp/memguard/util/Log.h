//
// Created by tomystang on 2020/11/24.
//

#ifndef __MEMGUARD_LOG_H__
#define __MEMGUARD_LOG_H__


#include <android/log.h>
#include "Auxiliary.h"
#include "LogFn.h"

#ifndef EnableLOG
    #undef MEMGUARD_LOG_LEVEL
    #define MEMGUARD_LOG_LEVEL LOG_SILENT
#else
    #ifndef MEMGUARD_LOG_LEVEL
        #define MEMGUARD_LOG_LEVEL LOG_VERBOSE
    #endif
#endif

#define LOG_VERBOSE 2
#define LOG_DEBUG 3
#define LOG_INFO 4
#define LOG_WARN 5
#define LOG_ERROR 6
#define LOG_FATAL 7
#define LOG_SILENT 8

#define PRINT_LOG(level, tag, fmt, args...) do { memguard::log::PrintLog(level, tag, fmt, ##args); } while (false)
#define PRINT_LOG_V(level, tag, fmt, va_args) do { memguard::log::PrintLogV(level, tag, fmt, va_args); } while (false)

static inline void OmitLog(const char* tag, const char* fmt, ...) { UNUSED(tag); UNUSED(fmt); }
static inline void OmitLogV(const char* tag, const char* fmt, va_list va_args) { UNUSED(tag); UNUSED(fmt); UNUSED(va_args); }

#if MEMGUARD_LOG_LEVEL <= LOG_VERBOSE
#define LOGV(tag, fmt, args...) PRINT_LOG(ANDROID_LOG_VERBOSE, tag, fmt, ##args)
#define LOGV_V(tag, fmt, va_args) PRINT_LOG_V(ANDROID_LOG_VERBOSE, tag, fmt, va_args)
#else
#define LOGV(tag, fmt, args...) OmitLog(tag, fmt, ##args)
#define LOGV_V(tag, fmt, va_args) OmitLogV(tag, fmt, va_args)
#endif

#if MEMGUARD_LOG_LEVEL <= LOG_DEBUG
#define LOGD(tag, fmt, args...) PRINT_LOG(ANDROID_LOG_DEBUG, tag, fmt, ##args)
#define LOGD_V(tag, fmt, va_args) PRINT_LOG_V(ANDROID_LOG_DEBUG, tag, fmt, va_args)
#else
#define LOGD(tag, fmt, args...) OmitLog(tag, fmt, ##args)
#define LOGD_V(tag, fmt, va_args) OmitLogV(tag, fmt, va_args)
#endif

#if MEMGUARD_LOG_LEVEL <= LOG_INFO
#define LOGI(tag, fmt, args...) PRINT_LOG(ANDROID_LOG_INFO, tag, fmt, ##args)
#define LOGI_V(tag, fmt, va_args) PRINT_LOG_V(ANDROID_LOG_INFO, tag, fmt, va_args)
#else
#define LOGI(tag, fmt, args...) OmitLog(tag, fmt, ##args)
#define LOGI_V(tag, fmt, va_args) OmitLogV(tag, fmt, va_args)
#endif

#if MEMGUARD_LOG_LEVEL <= LOG_WARN
#define LOGW(tag, fmt, args...) PRINT_LOG(ANDROID_LOG_WARN, tag, fmt, ##args)
#define LOGW_V(tag, fmt, va_args) PRINT_LOG_V(ANDROID_LOG_WARN, tag, fmt, va_args)
#else
#define LOGW(tag, fmt, args...) OmitLog(tag, fmt, ##args)
#define LOGW_V(tag, fmt, va_args) OmitLogV(tag, fmt, va_args)
#endif

#if MEMGUARD_LOG_LEVEL <= LOG_ERROR
#define LOGE(tag, fmt, args...) PRINT_LOG(ANDROID_LOG_ERROR, tag, fmt, ##args)
#define LOGE_V(tag, fmt, va_args) PRINT_LOG_V(ANDROID_LOG_ERROR, tag, fmt, va_args)
#else
#define LOGE(tag, fmt, args...) OmitLog(tag, fmt, ##args)
#define LOGE_V(tag, fmt, va_args) OmitLogV(tag, fmt, va_args)
#endif

#if MEMGUARD_LOG_LEVEL <= LOG_FATAL
#define LOGF(tag, fmt, args...) PRINT_LOG(ANDROID_LOG_FATAL, tag, fmt, ##args)
#define LOGF_V(tag, fmt, va_args) PRINT_LOG_V(ANDROID_LOG_FATAL, tag, fmt, va_args)
#else
#define LOGF(tag, fmt, args...) OmitLog(tag, fmt, ##args)
#define LOGF_V(tag, fmt, va_args) OmitLogV(tag, fmt, va_args)
#endif

#define ASSERT(cond, fmt, args...) \
    do { \
      if (UNLIKELY(!(cond))) { \
        __android_log_assert(nullptr, "MemGuard", "Assertion failed: %s, msg: " fmt, #cond, ##args); \
      } \
    } while (0)


#endif //__MEMGUARD_LOG_H__
