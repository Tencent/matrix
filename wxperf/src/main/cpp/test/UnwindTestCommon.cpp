#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include <inttypes.h>
#include "log.h"
#include "Backtrace.h"
#include "../external/libunwindstack/TimeUtil.h"
#include "UnwindTestCommon.h"

#define DWARF_UNWIND_TAG "Dwarf-Unwind"
#define FP_FAST_UNWIND_TAG "Fp-Unwind"

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
            } \
            LOGE(UNWIND_TEST_TAG, #tag" costs: %ldns", (tms.tv_nsec - timestamp)); \
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

    LOGD(DWARF_UNWIND_TAG, "frames = %llu", tmp_ns->size());

    for (auto p_frame = tmp_ns->begin(); p_frame != tmp_ns->end(); ++p_frame) {
        Dl_info stack_info;
        dladdr((void *) p_frame->pc, &stack_info);

        std::string so_name = std::string(stack_info.dli_fname);

        LOGE(DWARF_UNWIND_TAG, "  #pc 0x%"
        PRIx64
        " %"
        PRIu64
        " 0x%"
        PRIx64
        " %s (%s)", p_frame->rel_pc, p_frame->pc, p_frame->pc, stack_info.dli_sname, stack_info.dli_fname);
    }
}



inline void print_fp_unwind() {

    NanoSeconds_Start(nano);

    uptr frames[FRAME_MAX_SIZE];

    uptr frame_size = 0;

    wechat_backtrace::fp_fast_unwind(frames, FRAME_MAX_SIZE, frame_size);

    NanoSeconds_End(wechat_backtrace::fp_fast_unwind, nano);

    LOGE(FP_FAST_UNWIND_TAG, "frames = %llu", frame_size);

    for (size_t i = 0 ; i < frame_size; i++) {
        Dl_info stack_info;
        dladdr((void *) frames[i], &stack_info);

        std::string so_name = std::string(stack_info.dli_fname);

        LOGE(DWARF_UNWIND_TAG, "  #pc 0x%"
                PRIxPTR
                " %"
                PRIuPTR
                " 0x%"
                PRIxPTR
                " %s (%s)", frames[i], frames[i], frames[i], stack_info.dli_sname, stack_info.dli_fname);
    }
}

void leaf_func(const char * testcase) {

    LOGD(UNWIND_TEST_TAG, "Test %s unwind start with mode %d.", testcase, gMode);

    switch (gMode) {
        case FP_UNWIND:
            print_fp_unwind();
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