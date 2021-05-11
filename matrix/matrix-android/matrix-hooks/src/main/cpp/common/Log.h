//
// Created by Yves on 2021/4/27.
//

#ifndef MATRIX_HOOK_LOG_H
#define MATRIX_HOOK_LOG_H

#include <android/log.h>

#ifdef EnableLOG

#undef LOGD
#undef LOGI
#undef LOGE

#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) __android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#else

#define LOGD(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#endif

#ifndef LOG_ALWAYS_FATAL

#define __FAKE_USE_VA_ARGS(...) ((void)(0))
#define __android_second(dummy, second, ...) second
#define __android_rest(first, ...) , ##__VA_ARGS__

#define android_printAssert(cond, tag, ...)                     \
  __android_log_assert(cond, tag,                               \
                       __android_second(0, ##__VA_ARGS__, NULL) \
                           __android_rest(__VA_ARGS__))

#define LOG_ALWAYS_FATAL(LOG_TAG, ...) \
  (((void)android_printAssert(NULL, LOG_TAG, ##__VA_ARGS__)))
#endif

int flogger(FILE *fp, const char *fmt, ...);

#endif //MATRIX_HOOK_LOG_H
