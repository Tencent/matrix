//
// Created by Yves on 2020-05-12.
//

#include <jni.h>
#include "BenchMarkCommon.h"
#include "MemoryBenchmarkTest.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIMES 20

#define BENCHMARK_TIMES(test_func, times) \
    LOGD(MEMORY_BENCHMARK_TAG, "Start "#test_func" benchmark"); \
    for (int i = 0; i < (times); ++i) { \
        test_func(); \
    }

#define BENCHMARK(test_func) BENCHMARK_TIMES(test_func, TIMES)

JNIEXPORT void JNICALL
Java_com_tencent_mm_libwxperf_MemoryBenchmarkTest_benchmarkNative(JNIEnv *env,
                                                                             jclass clazz) {

    BENCHMARK(single_thread)
    BENCHMARK(cross_thread)
//    BENCHMARK(concurrent_alloc)

    LOGD(MEMORY_BENCHMARK_TAG, "start concurrent_alloc benchmark");
    concurrent_alloc();

}

#ifdef __cplusplus
}
#endif