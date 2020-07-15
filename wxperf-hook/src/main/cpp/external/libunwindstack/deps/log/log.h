//
// Created by tomystang on 2019/6/20.
//

#ifndef LIBUNWINDSTACK_LOG_H
#define LIBUNWINDSTACK_LOG_H


#include <sys/types.h>
#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG NULL
#endif

#define __PRINT_LOG(prio, tag, fmt, args...) \
  do { __android_log_print(prio, tag, fmt, ##args); } while (0)

#define __VPRINT_LOG(prio, tag, fmt, args) \
  do { __android_log_vprint(prio, tag, fmt, args); } while (0)

#ifndef LOG_PRI
  #define LOG_PRI(prio, tag, fmt, args...) __PRINT_LOG(prio, tag, fmt, ##args)
#endif

#ifndef LOG_PRI_VA
  #define LOG_PRI_VA(prio, tag, fmt, args) __VPRINT_LOG(prio, tag, fmt, args)
#endif

#define ALOGV(fmt, args...) __PRINT_LOG(ANDROID_LOG_VERBOSE, LOG_TAG, fmt, ##args)
#define ALOGD(fmt, args...) __PRINT_LOG(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
#define ALOGI(fmt, args...) __PRINT_LOG(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args)
#define ALOGW(fmt, args...) __PRINT_LOG(ANDROID_LOG_WARN, LOG_TAG, fmt, ##args)
#define ALOGE(fmt, args...) __PRINT_LOG(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##args)


#endif //LIBUNWINDSTACK_LOG_H
