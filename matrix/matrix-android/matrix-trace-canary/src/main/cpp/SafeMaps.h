
// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// SafeMaps.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SAFEMAPS_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SAFEMAPS_H_

#include <sys/types.h>
#include <functional>
#include <cstdint>
#include <string_view>

namespace MatrixTracer {

using ProcessMapsCallback = std::function<
        bool(uintptr_t, uintptr_t, uint16_t, uint64_t, ino_t, std::string_view)>;

bool readProcessMaps(pid_t pid, const ProcessMapsCallback& callback);

bool readMapsFile(const char *fileName, const ProcessMapsCallback& callback);

static bool readSelfMaps(const ProcessMapsCallback& callback) {
    return readMapsFile("/proc/self/maps", callback);
}


class SafeMaps {
 public:
    explicit SafeMaps(pid_t pid) : mPid(pid) {}
    virtual ~SafeMaps() = default;

    virtual bool Parse();

 private:
    pid_t mPid;
};

}   // namespace MatrixTracer

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SAFEMAPS_H_