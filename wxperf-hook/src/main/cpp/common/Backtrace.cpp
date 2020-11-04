//
// Created by Yves on 2019-08-09.
//

#include <dlfcn.h>
#include "Backtrace.h"
#include "JNICommon.h"
#include <FpUnwinder.h>
#include <MinimalRegs.h>
#include <FastArmExidxUnwinder.h>
#include <QuickenUnwinder.h>
#include <QuickenMaps.h>
#include <LocalMaps.h>

namespace wechat_backtrace {

    static pthread_mutex_t unwind_mutex = PTHREAD_MUTEX_INITIALIZER;

    void update_maps() {

        wechat_backtrace::UpdateLocalMaps();

#ifdef __arm__
        // Parse quicken maps
        wechat_backtrace::Maps::Parse();
#endif
    }

    void dwarf_unwind(unwindstack::Regs* regs, std::vector<unwindstack::FrameData> &dst, size_t frameSize) {

        pthread_mutex_lock(&unwind_mutex);
        if (regs == nullptr) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get remote reg data.");
            pthread_mutex_unlock(&unwind_mutex);
            return;
        }

        unwindstack::SetFastFlag(false);

        std::shared_ptr<unwindstack::LocalMaps> local_maps = GetMapsCache();
        if (!local_maps) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get maps.");
            pthread_mutex_unlock(&unwind_mutex);
            return;
        }

        auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
        unwindstack::Unwinder unwinder(frameSize, local_maps.get(), regs, process_memory);
        unwindstack::JitDebug jit_debug(process_memory);
        unwinder.SetJitDebug(&jit_debug, regs->Arch());
        unwinder.SetResolveNames(false);
        unwinder.Unwind();

        dst = unwinder.frames();

        pthread_mutex_unlock(&unwind_mutex);
    }

    void fp_unwind(uptr* regs, uptr* frames, uptr frameMaxSize, uptr &frameSize) {
        FpUnwind(regs, frames, frameMaxSize, frameSize, false);
    }

    void fp_unwind_with_fallback(uptr* regs, uptr* frames, uptr frameMaxSize, uptr &frameSize) {
        FpUnwind(regs, frames, frameMaxSize, frameSize, true);
    }

#ifdef __arm__
    void fast_dwarf_unwind(uptr* regs, uptr* frames, uptr frame_max_size, uptr &frame_size) {
        wechat_backtrace::FastExidxUnwind((uint32_t*)regs, frames, frame_max_size, frame_size);
    }
#else
    void fast_dwarf_unwind(unwindstack::Regs* regs, std::vector<unwindstack::FrameData> &dst, size_t frameSize) {

        pthread_mutex_lock(&unwind_mutex);
        if (regs == nullptr) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get remote reg data.");
            pthread_mutex_unlock(&unwind_mutex);
        }

        unwindstack::SetFastFlag(true);

        std::shared_ptr<unwindstack::LocalMaps> local_maps = GetMapsCache();
        if (!local_maps) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get maps.");
            pthread_mutex_unlock(&unwind_mutex);
            return;
        }

        auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
        unwindstack::Unwinder unwinder(frameSize, local_maps.get(), regs, process_memory);
        unwindstack::JitDebug jit_debug(process_memory);
        unwinder.SetJitDebug(&jit_debug, regs->Arch());
        unwinder.SetResolveNames(false);
        unwinder.Unwind();

        dst = unwinder.frames();

        pthread_mutex_unlock(&unwind_mutex);
    }

#endif

    void quicken_unwind(uptr* regs, uptr* frames, uptr frame_max_size, uptr &frame_size) {
#ifdef __arm__
        unwindstack::ArchEnum arch = unwindstack::ArchEnum::ARCH_ARM;
#else
        unwindstack::ArchEnum arch = unwindstack::ArchEnum::ARCH_ARM64;
#endif
        wechat_backtrace::WeChatQuickenUnwind(arch, regs, frames, frame_max_size, frame_size);
    }
}
