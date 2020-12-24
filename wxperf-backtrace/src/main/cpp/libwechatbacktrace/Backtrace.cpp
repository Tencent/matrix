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

#define WECHAT_BACKTRACE_TAG "Wehat.Backtrace"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    static pthread_mutex_t unwind_mutex = PTHREAD_MUTEX_INITIALIZER;

    static BacktraceMode backtrace_mode = FramePointer;

    BacktraceMode GetBacktraceMode() {
        return backtrace_mode;
    }

    void SetBacktraceMode(BacktraceMode mode) {
        backtrace_mode = mode;
    }

    void restore_frame_detail(const Frame *frames, const size_t frame_size,
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
                    real_pc - (uptr) stack_info.dli_fbase,
                    success == 0 || stack_info.dli_sname == nullptr ? "null" : stack_info.dli_sname,
                    success == 0 || stack_info.dli_fname == nullptr ? "null" : stack_info.dli_fname
            };
            frame_callback(detail);

            // TODO Deal with dex pc
        }

        return;
    }

    void notify_maps_changed() {

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

    void dwarf_unwind(unwindstack::Regs *regs, std::vector<unwindstack::FrameData> &dst,
                      size_t frameSize) {

        if (regs == nullptr) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get remote reg data.");
            return;
        }

        unwindstack::SetFastFlag(false);

        pthread_mutex_lock(&unwind_mutex);

        std::shared_ptr<unwindstack::LocalMaps> local_maps = GetMapsCache();
        if (!local_maps) {
            pthread_mutex_unlock(&unwind_mutex);
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get maps.");
            return;
        }

        auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
        unwindstack::Unwinder unwinder(frameSize, local_maps.get(), regs, process_memory);
        unwindstack::JitDebug jit_debug(process_memory);
        unwinder.SetJitDebug(&jit_debug, regs->Arch());
        unwinder.SetResolveNames(false);
        unwinder.Unwind();

        pthread_mutex_unlock(&unwind_mutex);

        dst = unwinder.frames();
    }

    void fp_unwind(uptr *regs, Frame *frames, size_t frameMaxSize, size_t &frameSize) {
        FpUnwind(regs, frames, frameMaxSize, frameSize);
    }


    QUT_EXTERN_C_BLOCK_END
}
