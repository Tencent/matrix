//
// Created by Yves on 2019-08-09.
//

#include <dlfcn.h>
#include <unwindstack/RegsArm64.h>
#include "StackTrace.h"
#include "JNICommon.h"

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

    void do_unwind(std::vector<FrameData> *dst) {
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

        *dst = unwinder.frames();
        pthread_mutex_unlock(&unwind_mutex);
    }
}
