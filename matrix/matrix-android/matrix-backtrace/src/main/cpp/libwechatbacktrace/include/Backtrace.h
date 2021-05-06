//
// Created by Yves on 2019-08-09.
//

#ifndef LIBWECHATBACKTRACE_BACKTRACE_H
#define LIBWECHATBACKTRACE_BACKTRACE_H

#include <unwindstack/Maps.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/RegsGetLocal.h>
#include <unwindstack/Unwinder.h>
#include <unwindstack/JitDebug.h>
#include <FpUnwinder.h>
#include <MinimalRegs.h>
#include <QuickenUnwinder.h>
#include <android/log.h>
#include <unwindstack/RegsArm.h>
#include "BacktraceDefine.h"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    BacktraceMode get_backtrace_mode();

    void set_backtrace_mode(BacktraceMode mode);

    void BACKTRACE_FUNC_WRAPPER(dwarf_unwind)(unwindstack::Regs *regs, std::vector<unwindstack::FrameData> &, size_t);

    void BACKTRACE_FUNC_WRAPPER(fp_unwind)(uptr *regs, Frame *frames, const size_t frameMaxSize, size_t &frameSize);

    void BACKTRACE_FUNC_WRAPPER(quicken_unwind)(uptr *regs, Frame *frames, const size_t frame_max_size, uptr &frame_size);

    void BACKTRACE_FUNC_WRAPPER(quicken_based_unwind)(Frame *frames, const size_t max_frames, size_t &frame_size);

    void BACKTRACE_FUNC_WRAPPER(fp_based_unwind)(Frame *frames, const size_t max_frames, size_t &frame_size);

    void BACKTRACE_FUNC_WRAPPER(dwarf_based_unwind)(Frame *frames, const size_t max_frames, size_t &frame_size);

    void BACKTRACE_FUNC_WRAPPER(unwind_adapter)(Frame *frames, const size_t max_frames, size_t &frame_size);

    void BACKTRACE_FUNC_WRAPPER(restore_frame_detail)(const Frame *frames, const size_t frame_size,
                                                      std::function<void(FrameDetail)> frame_callback);

    void BACKTRACE_FUNC_WRAPPER(notify_maps_changed)();

    QUT_EXTERN_C_BLOCK_END
}

#endif //LIBWECHATBACKTRACE_BACKTRACE_H
