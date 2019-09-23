//
// Created by Yves on 2019-08-09.
//

#include <dlfcn.h>
#include "StackTrace.h"

namespace unwindstack {

    static LocalMaps *localMaps;

    static bool has_inited = false;

    void update_maps() {
        if (!localMaps) {
            localMaps = new LocalMaps;
        } else {
            delete localMaps;
            localMaps = new LocalMaps;
        }
        if (!localMaps->Parse()) {
            LOGE("Yves.unwind", "Failed to parse map data.");
            return;
        } else {
            has_inited = true;
        }
    }

//    void do_unwind(uint64_t **p_stacks, size_t *p_stack_size) {
//        if (!has_inited) {
//            LOGE("Yves.unwind", "libunwindstack was not inited");
//            return;
//        }
//
//        static Regs *regs = Regs::CreateFromLocal();
//        RegsGetLocal(regs);
//        if (regs == nullptr) {
//            LOGE("Yves.unwind", "Unable to get remote reg data");
//            return;
//        }
//
//        auto process_memory = Memory::CreateProcessMemory(getpid());
//        Unwinder unwinder(128, localMaps, regs, process_memory);
//        JitDebug jit_debug(process_memory);
//        unwinder.SetJitDebug(&jit_debug, regs->Arch());
//        unwinder.Unwind();
//
//        LOGD("Yves.unwind", "unwind done");
//        // Print the frames.
//        *p_stack_size = unwinder.NumFrames();
//        *p_stacks = static_cast<uint64_t *>(malloc(sizeof(uint64_t) * (*p_stack_size + 1)));
//
//        auto *p_frames = new std::vector<FrameData>;
//        auto frames = unwinder.frames();
//        (*p_frames) = frames;
//
//        for (size_t i = 0; i < unwinder.NumFrames(); i++) {
//            (*p_stacks)[i] = frames[i].pc;
//        }
//    }

    void do_unwind(std::vector<FrameData> &dst) {
        if (!has_inited) {
            LOGE("Yves.unwind", "libunwindstack was not inited");
            return;
        }

        static Regs *regs = Regs::CreateFromLocal();
        RegsGetLocal(regs);
        if (regs == nullptr) {
            LOGE("Yves.unwind", "Unable to get remote reg data");
            return;
        }

        auto process_memory = Memory::CreateProcessMemory(getpid());
        Unwinder unwinder(128, localMaps, regs, process_memory);
        JitDebug jit_debug(process_memory);
        unwinder.SetJitDebug(&jit_debug, regs->Arch());
        unwinder.SetResolveNames(false);
        unwinder.Unwind();

//        for (size_t i = 0; i < unwinder.NumFrames(); i++) {
//            LOGD("Yves.unwind", "~~~~~~~~~~~~~%s", unwinder.FormatFrame(i).c_str());
//        }

        dst = unwinder.frames();
    }
}
