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
// logging.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_LOGGING_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_LOGGING_H_

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifdef ALOGV
#undef ALOGV
#endif

namespace MatrixTraffic {
namespace Logging {

static constexpr const char *kLogTag = "MatrixTraffic";

#if defined(__GNUC__)
__attribute__((__format__(printf, 1, 2)))
#endif
static inline void ALOGV(const char *fmt, ...) {
#ifdef __ANDROID__
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_VERBOSE, kLogTag, fmt, ap);
    va_end(ap);
#endif
}

#if defined(__GNUC__)
__attribute__((__format__(printf, 1, 2)))
#endif
static inline void ALOGI(const char *fmt, ...) {
#ifdef __ANDROID__
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, kLogTag, fmt, ap);
    va_end(ap);
#endif
}

#if defined(__GNUC__)
__attribute__((__format__(printf, 1, 2)))
#endif
static inline void ALOGE(const char *fmt, ...) {
#ifdef __ANDROID__
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, kLogTag, fmt, ap);
    va_end(ap);
#endif
}

}   // namespace Logging
}   // namespace MatrixTraffic

using ::MatrixTraffic::Logging::ALOGV;
using ::MatrixTraffic::Logging::ALOGI;
using ::MatrixTraffic::Logging::ALOGE;

#endif  // MATRIX_TRAFFIC_MAIN_CPP_LOGGING_H_
