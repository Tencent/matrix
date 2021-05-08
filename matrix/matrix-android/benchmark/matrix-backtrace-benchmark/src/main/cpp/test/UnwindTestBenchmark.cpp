#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cinttypes>
#include <memory>
#include <cxxabi.h>
#include <Backtrace.h>
#include <QuickenMaps.h>
#include <LocalMaps.h>
#include <QuickenUnwinder.h>
#include "Backtrace.h"

#include "BenchmarkLog.h"
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
    BENCHMARK_LOGE(UNWIND_TEST_TAG, "Start "#test_func" case benchmark for mode "#mode); \
    reset_benchmark_counting(); \
    for (int i = 0; i < times; i++) { \
        test_func(); \
        usleep(100); \
    } \
    dump_benchmark_calculation(#mode); \
};

#define BENCHMARK(mode, test_func) BENCHMARK_TIMES(mode, benchmark_times, test_func)

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeInit(JNIEnv *env,
                                                                      jclass clazz) {

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(notify_maps_changed)();
}


JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeBenchmark(JNIEnv *env, jclass clazz) {

    // DWARF_UNWIND mode benchmark
    BENCHMARK(DWARF_UNWIND, func_selfso)
//    BENCHMARK(DWARF_UNWIND, func_throughjni)
//    BENCHMARK(DWARF_UNWIND, func_throughsystemso)

    // FP_UNWIND mode benchmark
    BENCHMARK(FP_UNWIND, func_selfso);
//    BENCHMARK(FP_UNWIND, func_throughjni);
//    BENCHMARK(FP_UNWIND, func_throughsystemso);

    // QUICKEN_UNWIND mode benchmark
    BENCHMARK(WECHAT_QUICKEN_UNWIND, func_selfso);
//    BENCHMARK(WECHAT_QUICKEN_UNWIND, func_throughjni);
//    BENCHMARK(WECHAT_QUICKEN_UNWIND, func_throughsystemso);

}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeTry(JNIEnv *env, jclass clazz) {
    for (int i = 0; i < 1; i++) {
//    BENCHMARK_TIMES(DWARF_UNWIND, 1, func_selfso);
//    BENCHMARK_TIMES(FP_UNWIND, 1, func_selfso);
        BENCHMARK_TIMES(WECHAT_QUICKEN_UNWIND, 1, func_selfso);
    }

}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeRefreshMaps(JNIEnv *env, jclass clazz) {
    wechat_backtrace::notify_maps_changed();
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeUnwindAdapter(JNIEnv *env, jclass clazz) {
    BENCHMARK_TIMES(UNWIND_ADAPTER, 1, func_selfso);
}


#ifdef __cplusplus
}
#endif
