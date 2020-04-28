//
// Created by Yves on 2019-08-09.
//

#ifndef LIBWXPERF_JNI_STACKTRACE_H
#define LIBWXPERF_JNI_STACKTRACE_H

#include <unwindstack/Maps.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/RegsGetLocal.h>
#include <unwindstack/Unwinder.h>
#include <unwindstack/JitDebug.h>
#include <FastUnwinder.h>
#include "log.h"

#define WECHAT_BACKTRACE_TAG "Wechat.Backtrace"

namespace wechat_backtrace {

    void update_maps();

    void dwarf_unwind(std::vector<unwindstack::FrameData> &, size_t);

    void fp_fast_unwind(uptr * frames, uptr frameMaxSize, uptr &frameSize);

    void fp_unwind_with_fallback(uptr *frames, uptr frameMaxSize, uptr &frameSize);
}

#endif //LIBWXPERF_JNI_STACKTRACE_H
