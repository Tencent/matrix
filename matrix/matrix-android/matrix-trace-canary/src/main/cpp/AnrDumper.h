
// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// AnrDumper.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_ANRDUMPER_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_ANRDUMPER_H_

#include "SignalHandler.h"
#include <functional>
#include <string>
#include <optional>
#include <jni.h>

namespace MatrixTracer {

class AnrDumper : public SignalHandler {
 public:
    using DumpCallbackFunction = std::function<bool(int, const char *)>;

    AnrDumper(const char* anrTraceFile, const char* printTraceFile, DumpCallbackFunction&& callback);
    int doDump(bool isAnr);
    ~AnrDumper() = default;

 private:
    Result handleSignal(int sig, const siginfo_t *info, void *uc) final;

    const char* mAnrTraceFile;
    const char* mPrintTraceFile;

    const DumpCallbackFunction mCallback;
};
}   // namespace MatrixTracer

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_ANRDUMPER_H_