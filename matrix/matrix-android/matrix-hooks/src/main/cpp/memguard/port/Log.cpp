//
// Created by tomystang on 2020/11/24.
//

#include <android/log.h>
#include <util/Log.h>
// #include <common/Log.h>

using namespace memguard;

void memguard::log::PrintLog(int level, const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    PrintLogV(level, tag, fmt, args);
    va_end(args);
}

void memguard::log::PrintLogV(int level, const char* tag, const char* fmt, va_list args) {
    __android_log_vprint(level, tag, fmt, args);
    // internal_hook_vlogger(level, tag, fmt, args);
}