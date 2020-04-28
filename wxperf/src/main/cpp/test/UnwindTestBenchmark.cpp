#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include <Backtrace.h>
#include <MapsControll.h>
#include "log.h"
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
Java_com_tencent_mm_performance_jni_test_UnwindBenckmarkTest_benchmarkInitNative(JNIEnv *env, jclass clazz) {

    // for dwarf unwinder
    wechat_backtrace::update_maps();

    // for fp unwinder with fallback
    wechat_backtrace::GetMapsCache();
    wechat_backtrace::UpdateFallbackPCRange();

}


JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_test_UnwindBenckmarkTest_benchmarkNative(JNIEnv *env, jclass clazz) {

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
Java_com_tencent_mm_performance_jni_test_UnwindBenckmarkTest_debugNative(JNIEnv *env, jclass clazz) {
    BENCHMARK_TIMES(FP_UNWIND, 1, func_selfso);
}

#ifdef __cplusplus
}
#endif