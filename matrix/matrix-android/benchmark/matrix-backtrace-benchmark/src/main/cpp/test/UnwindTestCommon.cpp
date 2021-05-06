#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include <inttypes.h>
#include <MinimalRegs.h>
#include <QuickenUnwinder.h>
#include <QuickenMaps.h>
#include "Backtrace.h"
#include "UnwindTestCommon.h"
#include "BenchmarkLog.h"

#define DWARF_UNWIND_TAG "Dwarf-Unwind"
#define FP_UNWIND_TAG "Fp-Unwind"
#define WECHAT_BACKTRACE_TAG "WeChat-Quicken-Unwind"

#define FRAME_MAX_SIZE 60

#define TEST_NanoSeconds_Start(timestamp) \
        long timestamp = 0; \
        { \
            struct timespec tms; \
            if (clock_gettime(CLOCK_REALTIME, &tms)) { \
                BENCHMARK_LOGE(UNWIND_TEST_TAG, "Err: Get time failed."); \
            } else { \
                timestamp = tms.tv_nsec; \
            } \
        }

#define TEST_NanoSeconds_End(tag, timestamp) \
        { \
            struct timespec tms; \
            if (clock_gettime(CLOCK_REALTIME, &tms)) { \
                BENCHMARK_LOGE(UNWIND_TEST_TAG, "Err: Get time failed."); \
            } \
            BENCHMARK_LOGE(UNWIND_TEST_TAG, #tag" %ld(ns) - %ld(ns) = costs: %ld(ns)" \
                , tms.tv_nsec, timestamp, (tms.tv_nsec - timestamp)); \
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

    TEST_NanoSeconds_Start(nano);

    std::shared_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
    unwindstack::RegsGetLocal(regs.get());

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(dwarf_unwind)(regs.get(), *tmp_ns, FRAME_MAX_SIZE);

    TEST_NanoSeconds_End(unwindstack::dwarf_unwind, nano);

    BENCHMARK_LOGE(DWARF_UNWIND_TAG, "frames = %"
            PRIuPTR, tmp_ns->size());

    for (auto p_frame = tmp_ns->begin(); p_frame != tmp_ns->end(); ++p_frame) {
        Dl_info stack_info;
        dladdr((void *) p_frame->pc, &stack_info);

        std::string so_name = std::string(stack_info.dli_fname);

        BENCHMARK_LOGE(DWARF_UNWIND_TAG, "  #pc 0x%"
                PRIx64
                " rel_pc 0x%"
                PRIx64
                " fbase 0x%"
                PRIxPTR
                " cal_rel_pc 0x%"
                PRIxPTR
                " %s (%s)", p_frame->pc, p_frame->rel_pc, (uptr) stack_info.dli_fbase,
             (uptr) (p_frame->pc - (uptr) stack_info.dli_fbase), stack_info.dli_sname,
             stack_info.dli_fname);
    }
}

inline void print_fp_unwind() {


    TEST_NanoSeconds_Start(nano);

    wechat_backtrace::Frame frames[FRAME_MAX_SIZE];

    uptr frame_size = 0;

    uptr regs[4];
    GetFramePointerMinimalRegs(regs);

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(fp_unwind)(regs, frames, FRAME_MAX_SIZE, frame_size);

    TEST_NanoSeconds_End(wechat_backtrace::fp_unwind, nano);

    BENCHMARK_LOGE(FP_UNWIND_TAG, "frames = %"
            PRIuPTR, frame_size);

    for (size_t i = 0; i < frame_size; i++) {
        Dl_info stack_info;
        dladdr((void *) frames[i].pc, &stack_info);

        std::string so_name = std::string(stack_info.dli_fname);

        BENCHMARK_LOGE(FP_UNWIND_TAG, "  #pc 0x%"
                PRIxPTR
                " %"
                PRIxPTR
                " 0x%"
                PRIxPTR
                " %s (%s)", frames[i].pc, (uptr) stack_info.dli_fbase,
             frames[i].pc - (uptr) stack_info.dli_fbase, stack_info.dli_sname, stack_info.dli_fname);
    }
}

inline void print_wechat_quicken_unwind() {

    TEST_NanoSeconds_Start(nano);

    uptr regs[QUT_MINIMAL_REG_SIZE];
    GetQuickenMinimalRegs(regs);
    wechat_backtrace::Frame frames[FRAME_MAX_SIZE];
    uptr frame_size = 0;

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(quicken_unwind)(regs, frames, FRAME_MAX_SIZE, frame_size);

    TEST_NanoSeconds_End(print_wechat_quicken_unwind, nano);

    LOGE(WECHAT_BACKTRACE_TAG, "frames = %"
            PRIuPTR, frame_size);

    for (size_t i = 0; i < frame_size; i++) {
        Dl_info stack_info;
        dladdr((void *) frames[i].pc, &stack_info);

        if (!stack_info.dli_fname) {
            BENCHMARK_LOGE(WECHAT_BACKTRACE_TAG, "  #pc stack_info.dli_fname is null");
            continue;
        }
        std::string so_name = std::string(stack_info.dli_fname);

        BENCHMARK_LOGE(WECHAT_BACKTRACE_TAG, "  #pc 0x%"
                PRIxPTR
                " %"
                PRIuPTR
                " 0x%"
                PRIxPTR
                " %s (%s)", frames[i].pc, frames[i].pc, frames[i].pc, stack_info.dli_sname,
             stack_info.dli_fname);
    }
}

inline void print_unwind_adapter() {

    TEST_NanoSeconds_Start(nano);

    wechat_backtrace::Frame frames[FRAME_MAX_SIZE];
    size_t frame_size = 0;

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(unwind_adapter)(frames, FRAME_MAX_SIZE, frame_size);

    TEST_NanoSeconds_End(print_unwind_adapter, nano);

    BENCHMARK_LOGE(WECHAT_BACKTRACE_TAG, "frames = %"
            PRIuPTR, frame_size);

    for (size_t i = 0; i < frame_size; i++) {
        Dl_info stack_info;
        dladdr((void *) frames[i].pc, &stack_info);

        if (!stack_info.dli_fname) {
            BENCHMARK_LOGE(WECHAT_BACKTRACE_TAG, "  #pc stack_info.dli_fname is null");
            continue;
        }
        std::string so_name = std::string(stack_info.dli_fname);

        BENCHMARK_LOGE(WECHAT_BACKTRACE_TAG, "  #pc 0x%"
                PRIxPTR
                " %"
                PRIuPTR
                " 0x%"
                PRIxPTR
                " %s (%s)", frames[i].pc, frames[i].pc, frames[i].pc, stack_info.dli_sname,
             stack_info.dli_fname);
    }
}

void leaf_func(const char *testcase) {

    BENCHMARK_LOGD(UNWIND_TEST_TAG, "Test %s unwind start with mode %d.", testcase, gMode);

    switch (gMode) {
        case FP_UNWIND:
            print_fp_unwind();
            break;
        case WECHAT_QUICKEN_UNWIND:
            print_wechat_quicken_unwind();
            break;
        case DWARF_UNWIND:
            print_dwarf_unwind();
            break;
        case UNWIND_ADAPTER:
            print_unwind_adapter();
            break;
        default:
            BENCHMARK_LOGE(UNWIND_TEST_TAG, "Unknown test %s with mode %d.", testcase, gMode);
            break;
    }

    BENCHMARK_LOGD(UNWIND_TEST_TAG, "Test %s unwind finished.", testcase);
}

#ifdef __cplusplus
}
#endif