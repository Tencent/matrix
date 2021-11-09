/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
