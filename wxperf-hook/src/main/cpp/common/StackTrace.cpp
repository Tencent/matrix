//
// Created by Yves on 2019-08-09.
//

#include <dlfcn.h>
#include <unwindstack/RegsArm64.h>
#include <FastRegs.h>
#include "StackTrace.h"
#include "JNICommon.h"
#include "Backtrace.h"

namespace unwindstack {

    static LocalMaps *localMaps;

    static bool has_inited = false;

    static pthread_mutex_t unwind_mutex = PTHREAD_MUTEX_INITIALIZER;

    void update_maps() {
        pthread_mutex_lock(&unwind_mutex);
//        while (pthread_mutex_trylock(&unwind_mutex) != 0) {
//            LOGE("Yves.unwind", "try lock failed");
//            sleep(1);
//        }

        if (!localMaps) {
            localMaps = new LocalMaps;
        } else {
            delete localMaps;
            localMaps = new LocalMaps;
        }
        if (!localMaps->Parse()) {
            LOGE("Yves.unwind", "Failed to parse map data.");
            pthread_mutex_unlock(&unwind_mutex);
            return;
        } else {
            has_inited = true;
        }
        pthread_mutex_unlock(&unwind_mutex);
    }

    void do_unwind(std::vector<FrameData> &dst) {
        if (!has_inited) {
            LOGE("Yves.unwind", "libunwindstack was not inited");
            return;
        }

        struct timespec tms;

        if (clock_gettime(CLOCK_REALTIME,&tms)) {
            LOGE("Unwind-debug", "get time error");
        }

        pthread_mutex_lock(&unwind_mutex);

        Regs *regs = Regs::CreateFromLocal();
        RegsGetLocal(regs);
        if (regs == nullptr) {
            LOGE("Yves.unwind", "Unable to get remote reg data");
            pthread_mutex_unlock(&unwind_mutex);
            return;
        }

        auto process_memory = Memory::CreateProcessMemory(getpid());
        Unwinder unwinder(16, localMaps, regs, process_memory);
//        JitDebug jit_debug(process_memory);
//        unwinder.SetJitDebug(&jit_debug, regs->Arch());
        unwinder.SetResolveNames(false);

        long nano = tms.tv_nsec;

        unwinder.Unwind();

        delete regs;

        if (clock_gettime(CLOCK_REALTIME,&tms)) {
            LOGE("Unwind-debug", "get time error");
        }
        LOGE("Unwind-debug", "unwinder.Unwind() costs: %ld",(tms.tv_nsec - nano));

//        for (size_t i = 0; i < unwinder.NumFrames(); i++) {
//            LOGD("Yves.unwind", "~~~~~~~~~~~~~%s", unwinder.FormatFrame(i).c_str());
//        }

        dst = unwinder.frames();
        pthread_mutex_unlock(&unwind_mutex);
    }
}

static inline void fp_unwind(uptr *frames, size_t max_size, size_t &frame_size) {
    uptr regs[4];
    RegsMinimalGetLocal(regs);
    wechat_backtrace::fp_fast_unwind(regs, frames, max_size, frame_size);
}

#define MAX_FRAMES 16

void unwind_adapter(std::vector<unwindstack::FrameData> &dst) {
#ifdef __aarch64__

    uptr frames[MAX_FRAMES];
    size_t frame_size = 0;

    fp_unwind(frames, MAX_FRAMES, frame_size);

    LOGD("Wxperf.unwind", "unwind adapter: size %zu", frame_size);

    for (auto i = 0; i < frame_size; i++) {
        unwindstack::FrameData data;
        data.pc = frames[i];
        dst.__emplace_back(data);
    }

    return;
#endif

#ifdef __arm__

    unwindstack::do_unwind(dst);

    return;
#endif

    __android_log_assert("NOT SUPPORT ARCH", "Unwind", "NOT SUPPORT ARCH");
}

void restore_frame_data(std::vector<unwindstack::FrameData> &frames) {
#ifdef __aarch64__

    LOGD("Wxperf.unwind", "restore frame %zu", frames.size());
    for (auto& frame_data: frames) {
        Dl_info stack_info;
        dladdr((void *) frame_data.pc, &stack_info);

        frame_data.rel_pc = frame_data.pc - (uptr) stack_info.dli_fbase;
        frame_data.function_name = stack_info.dli_sname == nullptr ? "null" : stack_info.dli_sname;
        frame_data.map_name = stack_info.dli_fname == nullptr ? "null" : stack_info.dli_fname;
    }

    return;
#endif

#ifdef __arm__

    for (auto& frame_data: frames) {
        Dl_info stack_info;
        dladdr((void *) frame_data.pc, &stack_info);

        frame_data.function_name = stack_info.dli_sname == nullptr ? "null" : stack_info.dli_sname;
        frame_data.map_name = stack_info.dli_fname == nullptr ? "null" : stack_info.dli_fname;
    }

    return;

#endif
    return;
}

void notify_maps_change() {
#ifdef __arm__
    unwindstack::update_maps();
#endif
}
