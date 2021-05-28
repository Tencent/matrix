//
// Created by Yves on 2019-08-09.
//

#include <dlfcn.h>
#include "Backtrace.h"
#include <FpUnwinder.h>
#include <MinimalRegs.h>
#include <QuickenUnwinder.h>
#include <QuickenMaps.h>
#include <LocalMaps.h>
#include <QuickenTableManager.h>
#include <android-base/macros.h>

#define WECHAT_BACKTRACE_TAG "Wechat.Backtrace"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    static BacktraceMode backtrace_mode = FramePointer;
    static bool enable_jit_unwind = false;

    BacktraceMode get_backtrace_mode() {
        return backtrace_mode;
    }

    void set_backtrace_mode(BacktraceMode mode) {
        backtrace_mode = mode;
    }

    void set_jit_unwind_enable(bool enable) {
        enable_jit_unwind = enable;
    }

    bool get_jit_unwind_enable() {
        return enable_jit_unwind;
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(restore_frame_detail)(const Frame *frames, const size_t frame_size,
                                                 std::function<void(FrameDetail)> frame_callback) {
        LOGD(WECHAT_BACKTRACE_TAG, "Restore frame data size: %zu.", frame_size);
        if (frames == nullptr || frame_callback == nullptr) {
            return;
        }

        for (size_t i = 0; i < frame_size; i++) {
            auto &frame_data = frames[i];
            Dl_info stack_info{};
            int success = dladdr((void *) frame_data.pc,
                                 &stack_info); // 用修正后的 pc dladdr 会偶现 npe crash, 因此还是用 lr

#ifdef __aarch64__  // TODO Fix hardcode
            // fp_unwind 得到的 pc 除了第 0 帧实际都是 LR, arm64 指令长度都是定长 32bit, 所以 -4 以恢复 pc
            uptr real_pc = frame_data.pc - (i > 0 ? 4 : 0);
#else
            uptr real_pc = frame_data.pc;
#endif
            FrameDetail detail = {
                    .rel_pc = real_pc - (uptr) stack_info.dli_fbase,
                    .map_name = success == 0 || stack_info.dli_fname == nullptr ? "null"
                                                                                : stack_info.dli_fname,
                    .function_name = success == 0 || stack_info.dli_sname == nullptr ? "null"
                                                                                     : stack_info.dli_sname
            };
            frame_callback(detail);

            // TODO Deal with dex pc
        }

        return;
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(quicken_unwind)(uptr *regs, Frame *frames, const size_t frame_max_size,
                                           uptr &frame_size) {
        WeChatQuickenUnwind(CURRENT_ARCH, regs, frame_max_size, frames, frame_size);
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(quicken_based_unwind)(Frame *frames, const size_t max_frames,
                                                 size_t &frame_size) {
        uptr regs[QUT_MINIMAL_REG_SIZE];
        GetQuickenMinimalRegs(regs);
        WeChatQuickenUnwind(CURRENT_ARCH, regs, max_frames, frames, frame_size);
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(fp_based_unwind)(Frame *frames, const size_t max_frames,
                                            size_t &frame_size) {
        uptr regs[FP_MINIMAL_REG_SIZE];
        GetFramePointerMinimalRegs(regs);
        FpUnwind(regs, frames, max_frames, frame_size);
        return;
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(dwarf_based_unwind)(Frame *frames, const size_t max_frames,
                                               size_t &frame_size) {
        std::vector<unwindstack::FrameData> dst;
        unwindstack::RegsArm regs;
        RegsGetLocal(&regs);
        BACKTRACE_FUNC_WRAPPER(dwarf_unwind)(&regs, dst, max_frames);
        size_t i = 0;
        auto it = dst.begin();
        while (it != dst.end()) {
            frames[i].pc = it->pc;
            frames[i].is_dex_pc = it->is_dex_pc;
            i++;
            it++;
        }
        frame_size = dst.size();
        return;
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(unwind_adapter)(Frame *frames, const size_t max_frames,
                                           size_t &frame_size) {

        switch (get_backtrace_mode()) {
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
    }

    BACKTRACE_EXPORT void BACKTRACE_FUNC_WRAPPER(notify_maps_changed)() {

#ifdef __arm__
        switch (backtrace_mode) {
            case DwarfBased:
                wechat_backtrace::UpdateLocalMaps();
                break;
            case Quicken:
                // Parse quicken maps
                wechat_backtrace::Maps::Parse();
                break;
            default:
                break;
        }
#endif
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(dwarf_unwind)(unwindstack::Regs *regs,
                                         std::vector<unwindstack::FrameData> &dst,
                                         size_t frameSize) {

        std::shared_ptr<unwindstack::LocalMaps> local_maps = GetMapsCache();
        if (!local_maps) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get maps.");
            return;
        }

        auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
        unwindstack::Unwinder unwinder(frameSize, local_maps.get(), regs, process_memory);
        unwindstack::JitDebug jit_debug(process_memory);
        unwinder.SetJitDebug(&jit_debug, regs->Arch());
        unwinder.SetResolveNames(false);
        unwinder.Unwind();

        dst = unwinder.frames();
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(fp_unwind)(uptr *regs, Frame *frames, const size_t frameMaxSize,
                                      size_t &frameSize) {
        FpUnwind(regs, frames, frameMaxSize, frameSize);
    }

    QUT_EXTERN_C_BLOCK_END
}
