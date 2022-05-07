//
// Created by YinSheng Tang on 2021/9/22.
//

#include "util/FdSanWrapper.h"

extern "C" enum android_fdsan_error_level android_fdsan_get_error_level() __attribute__((__weak__));

extern "C" enum android_fdsan_error_level android_fdsan_set_error_level(enum android_fdsan_error_level new_level) __attribute__((__weak__));

android_fdsan_error_level AndroidFdSanGetErrorLevel() {
    if (!android_fdsan_get_error_level) {
        return android_fdsan_error_level::ANDROID_FDSAN_ERROR_LEVEL_DISABLED;
    }
    return android_fdsan_get_error_level();
}

android_fdsan_error_level AndroidFdSanSetErrorLevel(enum android_fdsan_error_level new_level) {
    if (!android_fdsan_set_error_level) {
        return android_fdsan_error_level::ANDROID_FDSAN_ERROR_LEVEL_DISABLED;
    }
    return android_fdsan_set_error_level(new_level);
}