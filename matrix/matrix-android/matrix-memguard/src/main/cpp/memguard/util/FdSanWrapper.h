//
// Created by YinSheng Tang on 2021/9/22.
//

#ifndef MATRIX_ANDROID_FDSANWRAPPER_H
#define MATRIX_ANDROID_FDSANWRAPPER_H


enum android_fdsan_error_level {
    // No errors.
    ANDROID_FDSAN_ERROR_LEVEL_DISABLED,
    // Warn once(ish) on error, and then downgrade to ANDROID_FDSAN_ERROR_LEVEL_DISABLED.
    ANDROID_FDSAN_ERROR_LEVEL_WARN_ONCE,
    // Warn always on error.
    ANDROID_FDSAN_ERROR_LEVEL_WARN_ALWAYS,
    // Abort on error.
    ANDROID_FDSAN_ERROR_LEVEL_FATAL,
};

/*
 * Get the error level.
 */
extern android_fdsan_error_level AndroidFdSanGetErrorLevel();

/*
 * Set the error level and return the previous state.
 *
 * Error checking is automatically disabled in the child of a fork, to maintain
 * compatibility with code that forks, blindly closes FDs, and then execs.
 *
 * In cases such as the zygote, where the child has no intention of calling
 * exec, call this function to reenable fdsan checks.
 *
 * This function is not thread-safe and does not synchronize with checks of the
 * value, and so should probably only be called in single-threaded contexts
 * (e.g. postfork).
 */
extern android_fdsan_error_level AndroidFdSanSetErrorLevel(enum android_fdsan_error_level new_level);


#endif //MATRIX_ANDROID_FDSANWRAPPER_H
