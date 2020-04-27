#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include <Backtrace.h>
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

#define BENCHMARK(mode, test_func) { \
    set_unwind_mode(mode); \
    LOGE(UNWIND_TEST_TAG, "Start "#test_func" case benchmark for mode "#mode); \
    for (int i = 0; i < benchmark_times; i++) { \
        test_func(); \
    } \
};

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_test_UnwindBenckmarkTest_benchmarkInitNative(JNIEnv *env, jclass clazz) {
    wechat_backtrace::update_maps();
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
//
//    benchmark(FP_UNWIND_WITH_FALLBACK, benchmark_times, func_selfso);
//    benchmark(FP_UNWIND_WITH_FALLBACK, benchmark_times, func_throughjni);
//    benchmark(FP_UNWIND_WITH_FALLBACK, benchmark_times, func_throughsystemso);
//
//    benchmark(FAST_DWARF_UNWIND, benchmark_times, func_selfso);
//    benchmark(FAST_DWARF_UNWIND, benchmark_times, func_throughjni);
//    benchmark(FAST_DWARF_UNWIND, benchmark_times, func_throughsystemso);
}


#ifdef __cplusplus
}
#endif