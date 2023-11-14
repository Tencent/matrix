//
// Created by M.D. on 2021/10/26.
//

#ifndef MATRIX_ANDROID_LOG_H
#define MATRIX_ANDROID_LOG_H

#include <android/log.h>

void print_log(int level, const char *tag, const char *format, ...);

#define _info_log(tag, fmt, args...) print_log(ANDROID_LOG_INFO, tag, fmt, ##args)
#define _error_log(tag, fmt, args...) print_log(ANDROID_LOG_ERROR, tag, fmt, ##args)

#endif