// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// SafeMaps.cpp

#include "SafeMaps.h"

#include <sys/mman.h>
#include <fcntl.h>

#include "Support.h"
#include "Logging.h"

#define O_RDONLY 00000000

namespace MatrixTracer {

    inline bool skipSpaces(const char *&buf, size_t &size) {
        const char *s = buf;
        if (*s != ' ') return false;
        do { ++s; } while (*s == ' ');
        size -= (s - buf);
        buf = s;
        return true;
    }

    bool readProcessMaps(pid_t pid, const ProcessMapsCallback& callback) {
        char buf[24];
        StringBuilder(buf, sizeof(buf)) << "/proc/" << pid << "/maps";

        return readMapsFile(buf, callback);
    }

    bool readMapsFile(const char *fileName, const ProcessMapsCallback& callback) {
        ScopedFileDescriptor fd(open(fileName, O_RDONLY, 0));
        if (!fd.valid()) {
            ALOGE("Unable to open map file: %s", fileName);
            return false;
        }

        auto wrongFormat = [](const char *line, const char *p) {
            ALOGE("Wrong format in pos %zd: %s", p - line, line);
            return false;
        };

        LineReader lr(fd.get());
        const char *line;
        size_t len;
        while (lr.getNextLine(&line, &len)) {
            // Start and end address
            const char *p = line;
            size_t l = len;
            uintptr_t start = Support::readHex(p, l);
            if (*p++ != '-') return wrongFormat(line, p);
            uintptr_t end = Support::readHex(p, l);
            if (!skipSpaces(p, l)) return wrongFormat(line, p);
            if (end < start) return wrongFormat(line, p);

            // Flags
            uint16_t flags = 0;
            if (*p == 'r') flags |= PROT_READ;
            else if (*p != '-') return wrongFormat(line, p);
            p++;
            if (*p == 'w') flags |= PROT_WRITE;
            else if (*p != '-') return wrongFormat(line, p);
            p++;
            if (*p == 'x') flags |= PROT_EXEC;
            else if (*p != '-') return wrongFormat(line, p);
            p++;
            if (*p != 's' && *p != 'p') return wrongFormat(line, p);
            p++;
            if (!skipSpaces(p, l)) return wrongFormat(line, p);

            // Page offset
            uint64_t pageOffset = Support::readHex(p, l);
            if (!skipSpaces(p, l)) return wrongFormat(line, p);

            // Major and minor device
            Support::readHex(p, l);
            if (*p++ != ':') return wrongFormat(line, p);
            Support::readHex(p, l);
            if (!skipSpaces(p, l)) return wrongFormat(line, p);

            // inode
            ino_t inode = Support::readUInt(p, l);
            if (*p != '\0' && !skipSpaces(p, l)) return wrongFormat(line, p);

            if (callback) {
                if (callback(start, end, flags, pageOffset, inode,
                             std::string_view(p, len - (p - line))))
                    return true;
            }
            lr.popLine(len);
        }
        return true;
    }

    bool SafeMaps::Parse() {
        return readProcessMaps(mPid,
                               [this](uintptr_t start, uintptr_t end, uint16_t flags, uint64_t pageOffset,
                                      ino_t inode, std::string_view name) {
                                   using namespace Support;
                                   return false;
                               });
    }
}   // namespace MatrixTracer
