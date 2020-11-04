//
// Created by Yves on 2019-08-09.
//

#include <dlfcn.h>
#include <unwindstack/RegsArm64.h>
#include <MinimalRegs.h>
#include <QuickenMaps.h>
#include "Stacktrace.h"
#include "JNICommon.h"
#include "Backtrace.h"

#define MAX_FRAMES 16

namespace unwindstack {
    void update_maps() {
        wechat_backtrace::update_maps();
    }

    void do_unwind(std::vector<FrameData> &dst) {

        std::shared_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
        unwindstack::RegsGetLocal(regs.get());

        wechat_backtrace::dwarf_unwind(regs.get(), dst, MAX_FRAMES);
    }
}

using namespace wechat_backtrace;

//static inline void fp_unwind(wechat_backtrace::uptr* regs, wechat_backtrace::uptr *frames, size_t max_size, size_t &frame_size) {
//    wechat_backtrace::fp_unwind(regs, frames, max_size, frame_size);
//}

void unwind_adapter(std::vector<unwindstack::FrameData> &dst) {
#ifdef __aarch64__

    wechat_backtrace::uptr frames[MAX_FRAMES];
    size_t frame_size = 0;

    wechat_backtrace::uptr regs[4];
    RegsMinimalGetLocal(regs);

    wechat_backtrace::fp_unwind(regs, frames, MAX_FRAMES, frame_size);

    LOGD("Wxperf.unwind", "unwind adapter: size %zu", frame_size);

    for (auto i = 0; i < frame_size; i++) {
        unwindstack::FrameData data;
        data.pc = frames[i];
        dst.emplace_back(data);
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
    for (int i = 0; i < frames.size(); i++) {
        auto &frame_data = frames[i];
        Dl_info stack_info;
        dladdr((void *) frame_data.pc, &stack_info); // 用修正后的 pc dladdr 会偶现 npe crash, 因此还是用 lr

        // fp_unwind 得到的 pc 除了第 0 帧实际都是 LR, arm64 指令长度都是定长 32bit, 所以 -4 以恢复 pc
        uintptr_t real_pc = frame_data.pc - (i > 0 ? 4 : 0);

        frame_data.rel_pc = real_pc - (uptr) stack_info.dli_fbase;
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
