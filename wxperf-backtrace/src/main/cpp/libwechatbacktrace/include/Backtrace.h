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
#include "Predefined.h"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    void dwarf_unwind(unwindstack::Regs *regs, std::vector<unwindstack::FrameData> &, size_t);

    void fp_unwind(uptr *regs, Frame *frames, const size_t frameMaxSize, const size_t &frameSize);

    inline void quicken_unwind(uptr *regs, Frame *frames, uptr frame_max_size, uptr &frame_size) {
        WeChatQuickenUnwind(CURRENT_ARCH, regs, frame_max_size, frames, frame_size);
    }

    inline void unwind_adapter(Frame *frames, const size_t max_frames, size_t &frame_size) {

#ifdef __aarch64__
        uptr regs[4];
        RegsMinimalGetLocal(regs);
        FpUnwind(regs, frames, max_frames, frame_size);
        return;
#endif

#ifdef __arm__
        uptr regs[QUT_MINIMAL_REG_SIZE];
        GetMinimalRegs(regs);
        unwindstack::ArchEnum arch = unwindstack::ArchEnum::ARCH_ARM64;
        WeChatQuickenUnwind(arch, regs, max_frames, frames, frame_size);
        return;
#endif
        __android_log_assert("NOT SUPPORT ARCH", "WeChatBacktrace", "NOT SUPPORT ARCH");
    }

    void restore_frame_detail(const Frame *frames, const size_t frame_size,
                              std::function<void(FrameDetail)> frame_callback);

    void notify_maps_changed();

    QUT_EXTERN_C_BLOCK_END
}

#endif //LIBWECHATBACKTRACE_BACKTRACE_H
