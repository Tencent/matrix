//
// Created by yves on 20-4-11.
//

#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include "log.h"
#include "StackTrace.h"
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

inline void benchmark(UnwindTestMode mode, int round, void (*test_func) ()) {

    set_unwind_mode(mode);
    for (int i = 0; i < round; i++) {
        test_func();
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_test_UnwindTest_benchmark(JNIEnv *env, jclass clazz) {

    // DWARF_UNWIND mode benchmark
    benchmark(DWARF_UNWIND, benchmark_times, func_selfso);
    benchmark(DWARF_UNWIND, benchmark_times, func_throughjni);
    benchmark(DWARF_UNWIND, benchmark_times, func_throughsystemso);

//    benchmark(FP_UNWIND, benchmark_times, func_selfso);
//    benchmark(FP_UNWIND, benchmark_times, func_throughjni);
//    benchmark(FP_UNWIND, benchmark_times, func_throughsystemso);
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