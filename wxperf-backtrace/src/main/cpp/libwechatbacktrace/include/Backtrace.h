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
#include "Log.h"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    BacktraceMode GetBacktraceMode();

    void SetBacktraceMode(BacktraceMode mode);

    void dwarf_unwind(unwindstack::Regs *regs, std::vector<unwindstack::FrameData> &, size_t);

    void fp_unwind(uptr *regs, Frame *frames, const size_t frameMaxSize, const size_t &frameSize);

    inline void quicken_unwind(uptr *regs, Frame *frames, uptr frame_max_size, uptr &frame_size) {
        WeChatQuickenUnwind(CURRENT_ARCH, regs, frame_max_size, frames, frame_size);
    }

    inline void quicken_based_unwind(Frame *frames, const size_t max_frames, size_t &frame_size) {
        uptr regs[QUT_MINIMAL_REG_SIZE];
        GetQuickenMinimalRegs(regs);
        WeChatQuickenUnwind(CURRENT_ARCH, regs, max_frames, frames, frame_size);
    }

    inline void fp_based_unwind(Frame *frames, const size_t max_frames, size_t &frame_size) {
        uptr regs[FP_MINIMAL_REG_SIZE];
        GetFramePointerMinimalRegs(regs);
        FpUnwind(regs, frames, max_frames, frame_size);
        return;
    }

    inline void dwarf_based_unwind(Frame *frames, const size_t max_frames, size_t &frame_size) {
        std::vector<unwindstack::FrameData> dst;
        unwindstack::RegsArm regs;
        RegsGetLocal(&regs);
        dwarf_unwind(&regs, dst, max_frames);
        size_t i = 0;
        auto it = dst.begin();
        while(it != dst.end()) {
            frames[i].pc = it->pc;
            frames[i].is_dex_pc = it->is_dex_pc;
            i++;
            it++;
        }
        frame_size = dst.size();
        return;
    }

    inline void unwind_adapter(Frame *frames, const size_t max_frames, size_t &frame_size) {

#ifdef __aarch64__
        uptr regs[FP_MINIMAL_REG_SIZE];
        GetFramePointerMinimalRegs(regs);
        FpUnwind(regs, frames, max_frames, frame_size);
        return;
#endif

#ifdef __arm__
        switch (GetBacktraceMode()) {
            case FramePointer:
                fp_based_unwind(frames, max_frames, frame_size);
                break;
            case Quicken:
                quicken_based_unwind(frames, max_frames, frame_size);
                break;
            case DwarfBased:
                dwarf_based_unwind(frames, max_frames, frame_size);
                break;
        }
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
