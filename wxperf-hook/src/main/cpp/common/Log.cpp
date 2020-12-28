//
// Created by Yves on 2020/12/28.
//

#include "Log.h"

int flogger(FILE* fp , const char* fmt, ...) {
    if (!fp) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    int ret = vfprintf(fp, fmt, args);
    va_end(args);
    return ret;
}

