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
#include "logger.h"

namespace unwindstack {

    void update_maps();

//    void do_unwind(uint64_t **, size_t *);

    void do_unwind(std::vector<FrameData> &);
}

#endif //LIBWXPERF_JNI_STACKTRACE_H
