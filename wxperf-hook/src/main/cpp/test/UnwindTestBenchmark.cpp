#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <memory>
#include <cxxabi.h>
#include <Backtrace.h>
#include <QuickenMaps.h>
#include <FpFallbackUnwinder.h>
#include <LocalMaps.h>
#include <QuickenUnwinder.h>
#include "Log.h"
#include "Backtrace.h"
#include "../external/libunwindstack/TimeUtil.h"

#include "UnwindTestCase_SelfSo.h"
#include "UnwindTestCase_ThroughJNI.h"
#include "UnwindTestCase_TrhoughSystemSo.h"
#include "UnwindTestCommon.h"

#define benchmark_times 20

#ifdef __cplusplus
extern "C" {
#endif

#define BENCHMARK_TIMES(mode, times, test_func) {\
    set_unwind_mode(mode); \
    LOGE(UNWIND_TEST_TAG, "Start "#test_func" case benchmark for mode "#mode); \
    for (int i = 0; i < times; i++) { \
        test_func(); \
    } \
};

#define BENCHMARK(mode, test_func) BENCHMARK_TIMES(mode, benchmark_times, test_func)

JNIEXPORT void JNICALL
Java_com_tencent_wxperf_jni_test_UnwindBenckmarkTest_benchmarkInitNative(JNIEnv *env,
                                                                         jclass clazz) {

    // for dwarf unwinder
    wechat_backtrace::UpdateLocalMaps();

    // for WeChat quicken unwinder
    wechat_backtrace::Maps::Parse();

    // for fp unwinder with fallback
    wechat_backtrace::UpdateFallbackPCRange();

    // enable Elf caching
    unwindstack::Elf::SetCachingEnabled(true);
}


JNIEXPORT void JNICALL
Java_com_tencent_wxperf_jni_test_UnwindBenckmarkTest_benchmarkNative(JNIEnv *env, jclass clazz) {

    // DWARF_UNWIND mode benchmark
    BENCHMARK(DWARF_UNWIND, func_selfso)
    BENCHMARK(DWARF_UNWIND, func_throughjni)
    BENCHMARK(DWARF_UNWIND, func_throughsystemso)

    // FP_UNWIND mode benchmark
    BENCHMARK(FP_UNWIND, func_selfso);
    BENCHMARK(FP_UNWIND, func_throughjni);
    BENCHMARK(FP_UNWIND, func_throughsystemso);

    // FP_UNWIND_WITH_FALLBACK mode benchmark
    BENCHMARK(FP_UNWIND_WITH_FALLBACK, func_selfso);
    BENCHMARK(FP_UNWIND_WITH_FALLBACK, func_throughjni);
    BENCHMARK(FP_UNWIND_WITH_FALLBACK, func_throughsystemso);

    // FAST_DWARF_UNWIND mode benchmark
    BENCHMARK(FAST_DWARF_UNWIND, func_selfso);
    BENCHMARK(FAST_DWARF_UNWIND, func_throughjni);
    BENCHMARK(FAST_DWARF_UNWIND, func_throughsystemso);
}

JNIEXPORT void JNICALL
Java_com_tencent_wxperf_jni_test_UnwindBenckmarkTest_debugNative(JNIEnv *env, jclass clazz) {
//    BENCHMARK_TIMES(DWARF_UNWIND, 1, func_selfso);
//    BENCHMARK_TIMES(DWARF_UNWIND, 1, func_selfso);
//    BENCHMARK_TIMES(FP_UNWIND, 1, func_selfso);
//    BENCHMARK_TIMES(FP_UNWIND_WITH_FALLBACK, 1, func_selfso);
//    BENCHMARK_TIMES(FAST_DWARF_UNWIND, 1, func_selfso);

    BENCHMARK_TIMES(DWARF_UNWIND, 10, func_selfso)
//    BENCHMARK_TIMES(DWARF_UNWIND, 1, func_selfso)
//    BENCHMARK_TIMES(FAST_DWARF_UNWIND, 10, func_selfso)

    BENCHMARK_TIMES(WECHAT_QUICKEN_UNWIND, 10, func_selfso);
//#ifdef __arm__
//    BENCHMARK_TIMES(WECHAT_QUICKEN_UNWIND_V2_WIP, 1, func_selfso);
//#endif

//    BENCHMARK_TIMES(FP_UNWIND, 3, func_selfso)
//    BENCHMARK_TIMES(FP_UNWIND, 3, func_throughjni)
//    BENCHMARK_TIMES(FP_UNWIND, 3, func_throughsystemso)

}

#ifdef __cplusplus
}
#endif
