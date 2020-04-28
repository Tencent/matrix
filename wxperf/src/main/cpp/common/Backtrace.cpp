//
// Created by Yves on 2019-08-09.
//

#include <dlfcn.h>
#include "Backtrace.h"
#include "JNICommon.h"
#include <FastUnwinder.h>

namespace wechat_backtrace {

    static unwindstack::LocalMaps *localMaps;

    static bool has_inited = false;

    static pthread_mutex_t unwind_mutex = PTHREAD_MUTEX_INITIALIZER;

    void update_maps() {
        pthread_mutex_lock(&unwind_mutex);

        if (!localMaps) {
            localMaps = new unwindstack::LocalMaps;
        } else {
            delete localMaps;
            localMaps = new unwindstack::LocalMaps;
        }
        if (!localMaps->Parse()) {
            LOGE(WECHAT_BACKTRACE_TAG, "Failed to parse map data.");
            pthread_mutex_unlock(&unwind_mutex);
            return;
        } else {
            has_inited = true;
        }
        pthread_mutex_unlock(&unwind_mutex);
    }

    void dwarf_unwind(unwindstack::Regs *regs, std::vector<unwindstack::FrameData> &dst, size_t frameSize) {

        if (!has_inited) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: libunwindstack was not initialized.");
            return;
        }

        pthread_mutex_lock(&unwind_mutex);
        if (regs == nullptr) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get remote reg data.");
            pthread_mutex_unlock(&unwind_mutex);
        }

        auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
        unwindstack::Unwinder unwinder(frameSize, localMaps, regs, process_memory);
        unwindstack::JitDebug jit_debug(process_memory);
        unwinder.SetJitDebug(&jit_debug, regs->Arch());
        unwinder.SetResolveNames(false);
        unwinder.Unwind();

        delete regs;

        dst = unwinder.frames();

        pthread_mutex_unlock(&unwind_mutex);
    }

    void fp_fast_unwind(uptr *regs, uptr *frames, uptr frameMaxSize, uptr &frameSize) {

        pthread_mutex_lock(&unwind_mutex);

        FpUnwind(regs, frames, frameMaxSize, frameSize, false);

        pthread_mutex_unlock(&unwind_mutex);
    }

    void fp_unwind_with_fallback(uptr *regs, uptr *frames, uptr frameMaxSize, uptr &frameSize) {

        pthread_mutex_lock(&unwind_mutex);

        FpUnwind(regs, frames, frameMaxSize, frameSize, true);

        pthread_mutex_unlock(&unwind_mutex);
    }
}
