
// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// SignalHandler.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SIGNALHANDLER_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SIGNALHANDLER_H_

#include <signal.h>

namespace MatrixTracer {

class SignalHandler {
 public:
    SignalHandler();
    virtual ~SignalHandler();

 protected:
    enum Result { NOT_HANDLED = 0, HANDLED, HANDLED_NO_RETRIGGER };
    virtual Result handleSignal(int sig, const siginfo_t *info, void *uc) = 0;

 private:
    static void signalHandler(int sig, siginfo_t* info, void* uc);
    static bool installHandlersLocked();

    SignalHandler(const SignalHandler &) = delete;
    SignalHandler &operator= (const SignalHandler &) = delete;
};

}   // namespace MatrixTracer

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_SIGNALHANDLER_H_
