//
// Created by YinSheng Tang on 2021/6/19.
//

#include <cstdio>
#include <cinttypes>
#include <cwctype>
#include "Log.h"
#include "Maps.h"

#define LOG_TAG "Matrix.Maps"

using namespace matrix;

bool matrix::IterateMaps(const MapsEntryCallback& cb, void* args) {
    if (cb == nullptr) {
        return false;
    }

    FILE* fp = nullptr;
    char line[PATH_MAX] = {};

    if ((fp = ::fopen("/proc/self/maps", "r")) == nullptr) {
        LOGE(LOG_TAG, "Fail to open /proc/self/maps");
        return false;
    }

    while(::fgets(line, sizeof(line), fp) != nullptr) {
        uintptr_t start = 0;
        uintptr_t end = 0;
        char perm[4] = {};
        int pathnamePos = 0;

        if (::sscanf(line, "%" PRIxPTR "-%" PRIxPTR " %4c %*x %*x:%*x %*d%n", &start, &end, perm, &pathnamePos) != 3) {
            continue;
        }

        if (pathnamePos <= 0) {
            continue;
        }
        while (::isspace(line[pathnamePos]) && pathnamePos <= static_cast<int>(sizeof(line) - 1)) {
            ++pathnamePos;
        }
        if (pathnamePos > static_cast<int>(sizeof(line) - 1)) {
            continue;
        }
        size_t pathLen = ::strlen(line + pathnamePos);
        if (pathLen == 0 || pathLen > static_cast<int>(sizeof(line) - 1)) {
            continue;
        }
        char* pathname = line + pathnamePos;
        while (pathLen > 0 && pathname[pathLen - 1] == '\n') {
            pathname[pathLen - 1] = '\0';
            --pathLen;
        }

        if (cb(start, end, perm, pathname, args)) {
            ::fclose(fp);
            return true;
        }
    }

    ::fclose(fp);
    return false;
}