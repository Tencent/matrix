
// Copyright 2020 Tencent Inc.  All rights reserved.
//
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

namespace MatrixTracer {
namespace Logging {

static constexpr const char *kLogTag = "MatrixTracer";

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
}   // namespace MatrixTracer

using ::MatrixTracer::Logging::ALOGV;
using ::MatrixTracer::Logging::ALOGI;
using ::MatrixTracer::Logging::ALOGE;

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_LOGGING_H_
