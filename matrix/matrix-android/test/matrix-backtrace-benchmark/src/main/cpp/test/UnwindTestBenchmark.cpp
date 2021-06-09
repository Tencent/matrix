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
#include "UnwindTestCommon.h"
#include "JavaStacktrace.h"

#define benchmark_times 20

#ifdef __cplusplus
extern "C" {
#endif

#define BENCHMARK_TIMES(mode, times, test_func) {\
    set_unwind_mode(mode); \
    BENCHMARK_RESULT_LOGE(UNWIND_TEST_TAG, "Start "#test_func" case benchmark for mode "#mode); \
    reset_benchmark_counting(); \
    for (int i = 0; i < times; i++) { \
        test_func; \
        usleep(200); \
    } \
    dump_benchmark_calculation(#mode); \
    usleep(200); \
    dump_benchmark_calculation(#mode); \
    usleep(200); \
    dump_benchmark_calculation(#mode); \
    usleep(2000); \
};

#define BENCHMARK(mode, test_func) BENCHMARK_TIMES(mode, benchmark_times, test_func)

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeInit(
        JNIEnv *env, jclass clazz) {
    wechat_backtrace::UpdateLocalMaps();
    wechat_backtrace::Maps::Parse();
}


JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeBenchmark(
        JNIEnv *env, jclass clazz) {

    benchmark_warm_up();

    size_t times = 10000;

    if (FRAME_MAX_SIZE >= 100) {
        BENCHMARK_TIMES(JAVA_UNWIND_PRINT_STACKTRACE, times, func_selfso());
        BENCHMARK_TIMES(QUICKEN_UNWIND_PRINT_STACKTRACE, times, func_selfso());
    } else if (FRAME_MAX_SIZE >= 60) {
        BENCHMARK_TIMES(DWARF_UNWIND, times, func_selfso());
        BENCHMARK_TIMES(WECHAT_QUICKEN_UNWIND, times, func_selfso());
    } else {
        BENCHMARK_TIMES(DWARF_UNWIND, times, func_selfso());
        BENCHMARK_TIMES(FP_UNWIND, times, func_selfso());
        BENCHMARK_TIMES(WECHAT_QUICKEN_UNWIND, times, func_selfso());
    }
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeBenchmarkJavaStack(
        JNIEnv *env, jclass clazz) {

    benchmark_warm_up();

    size_t times = 10000;
    BENCHMARK_TIMES(DWARF_UNWIND, times, print_dwarf_unwind());
    BENCHMARK_TIMES(JAVA_UNWIND, times, print_java_unwind());
    BENCHMARK_TIMES(WECHAT_QUICKEN_UNWIND, times, print_quicken_unwind());
    BENCHMARK_TIMES(JAVA_UNWIND_PRINT_STACKTRACE, times, print_java_unwind_formatted());
    BENCHMARK_TIMES(QUICKEN_UNWIND_PRINT_STACKTRACE, times, print_quicken_unwind_stacktrace());
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeTry(JNIEnv *env, jclass clazz) {

    bool previous = switch_print_stack(true);
    for (int i = 0; i < 1; i++) {
        BENCHMARK_TIMES(DWARF_UNWIND, 1, func_selfso());
        BENCHMARK_TIMES(FP_UNWIND, 1, func_selfso());
        BENCHMARK_TIMES(WECHAT_QUICKEN_UNWIND, 1, func_selfso());
    }
    switch_print_stack(previous);
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_benchmark_test_UnwindBenchmarkTest_nativeRefreshMaps(JNIEnv *env,
                                                                             jclass clazz) {
    wechat_backtrace::UpdateLocalMaps();
    wechat_backtrace::Maps::Parse();
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void) reserved;
    JNIEnv *env;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (env) {
        setJavaVM(vm);
    }

    return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif
