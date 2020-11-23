//
// Created by Yves on 2019-08-09.
//

#ifndef LIBWXPERF_JNI_STACKTRACE_H
#define LIBWXPERF_JNI_STACKTRACE_H

#include <unwindstack/Maps.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsGetLocal.h>
#include <unwindstack/Unwinder.h>
#include <unwindstack/JitDebug.h>
#include "Log.h"

namespace unwindstack {

    void update_maps();

//    void do_unwind(std::vector<FrameData> &);
}

//void unwind_adapter(std::vector<unwindstack::FrameData> &dst);
//
//void restore_frame_data(std::vector<unwindstack::FrameData> &frames);
//
//void notify_maps_change();

#endif //LIBWXPERF_JNI_STACKTRACE_H
