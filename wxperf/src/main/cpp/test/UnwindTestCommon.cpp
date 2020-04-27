//
// Created by yves on 20-4-11.
//

#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include "log.h"
#include "Backtrace.h"
#include "../external/libunwindstack/TimeUtil.h"
#include "UnwindTestCommon.h"

#define DWARF_UNWIND_TAG "Dwarf-Unwind"

#define FRAME_MAX_SIZE 16

#define NanoSeconds_Start(timestamp) \
        long timestamp = 0; \
        { \
            struct timespec tms; \
            if (clock_gettime(CLOCK_REALTIME, &tms)) { \
                LOGE(UNWIND_TEST_TAG, "Err: Get time failed."); \
            } else { \
                timestamp = tms.tv_nsec; \
            } \
        }

#define NanoSeconds_End(tag, timestamp) \
        { \
            struct timespec tms; \
            if (clock_gettime(CLOCK_REALTIME, &tms)) { \
                LOGE(UNWIND_TEST_TAG, "Err: Get time failed."); \
                uint64_t timestamp = 0; \
            } \
            LOGE(UNWIND_TEST_TAG, "tag costs: %ld", (tms.tv_nsec - timestamp)); \
        }

#ifdef __cplusplus
extern "C" {
#endif

static UnwindTestMode gMode = DWARF_UNWIND;

void set_unwind_mode(UnwindTestMode mode) {
    gMode = mode;
}

inline void print_dwarf_unwind() {
    auto *tmp_ns = new std::vector<unwindstack::FrameData>;

    NanoSeconds_Start(nano);

    wechat_backtrace::dwarf_unwind(*tmp_ns, FRAME_MAX_SIZE);

    NanoSeconds_End(unwindstack::dwarf_unwind, nano);

    LOGD(DWARF_UNWIND_TAG, "frames = %d", tmp_ns->size());

    for (auto p_frame = tmp_ns->begin(); p_frame != tmp_ns->end(); ++p_frame) {
        Dl_info stack_info;
        dladdr((void *) p_frame->pc, &stack_info);

        std::string so_name = std::string(stack_info.dli_fname);

        LOGE(DWARF_UNWIND_TAG, "  #pc %llx %llu %llx %s (%s)", p_frame->rel_pc, p_frame->pc, p_frame->pc, stack_info.dli_sname, stack_info.dli_fname);
    }
}

void leaf_func(const char * testcase) {

    LOGD(UNWIND_TEST_TAG, "Test %s unwind start with mode %d.", testcase, gMode);

    switch (gMode) {
        case FP_UNWIND:
            LOGE(UNWIND_TEST_TAG, "FP_UNWIND not supported yet.");
            break;
        case FP_UNWIND_WITH_FALLBACK:
            LOGE(UNWIND_TEST_TAG, "FP_UNWIND_WITH_FALLBACK not supported yet.");
            break;
        case FAST_DWARF_UNWIND_WITHOUT_JIT:
            LOGE(UNWIND_TEST_TAG, "FAST_DWARF_UNWIND_WITHOUT_JIT not supported yet.");
            break;
        case FAST_DWARF_UNWIND:
            LOGE(UNWIND_TEST_TAG, "FAST_DWARF_UNWIND not supported yet.");
            break;
        case DWARF_UNWIND:
            print_dwarf_unwind();
            break;
        default:
            LOGE(UNWIND_TEST_TAG, "Unknown test %s with mode %d.", testcase, gMode);
            break;
    }

    LOGD(UNWIND_TEST_TAG, "Test %s unwind finished.", testcase);
}

#ifdef __cplusplus
}
#endif