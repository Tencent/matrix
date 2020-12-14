//
// Created by Yves on 2020-05-12.
//

#include <jni.h>
#include <cstdlib>
#include "BenchMarkCommon.h"
#include "MemoryBenchmarkTest.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIMES 20
#define TAG "MemoryBenchmark"

#define BENCHMARK_TIMES(test_func, times) \
    LOGD(MEMORY_BENCHMARK_TAG, "Start "#test_func" benchmark"); \
    for (int i = 0; i < (times); ++i) { \
        test_func(); \
    }

#define BENCHMARK(test_func) BENCHMARK_TIMES(test_func, TIMES)

extern int benchmark_entry(int argc, const char **argv);

JNIEXPORT void JNICALL
Java_com_tencent_wxperf_sample_MemoryBenchmarkTest_benchmarkNative(JNIEnv *env,
                                                                  jclass clazz) {

//    BENCHMARK(single_thread)
//    BENCHMARK(cross_thread)
//    BENCHMARK(concurrent_alloc)



//    LOGD(MEMORY_BENCHMARK_TAG, "start concurrent_alloc benchmark");
//    concurrent_alloc();

#define THREAD_COUNT    "20"    /* Number of execution threads */
#define MODE            "0"     /* 0 for random size [min, max], 1 for fixed size (min) */
#define SIZE_MODE       "1"     /*  0 for even distribution, 1 for linear dropoff, 2 for exp dropoff */
#define CROSS_RATE      "2"    /* Rate of cross-thread deallocations (every n iterations), 0 for none */
#define LOOPS           "20"     /* Number of loops in each iteration (0 for default, 800k) */
#define ALLOCS          "10240"     /* Number of concurrent allocations in each thread, (0 for default, 10k) */
#define OP_COUNT        "5000"     /* Iteration operation count */
#define MIN_SIZE        "16"    /* Minimum size for random mode, fixed size for fixed mode */
#define MAX_SIZE        "8192"  /* Maximum size for random mode, ignored for fixed mode */

    const char *argv[] = {
            "benchmark",
            THREAD_COUNT,
            MODE,
            SIZE_MODE,
            CROSS_RATE,
            LOOPS,
            ALLOCS,
            OP_COUNT,
            MIN_SIZE,
            MAX_SIZE
    };

    benchmark_entry(10, argv);

    LOGD(TAG, "benchmark finished");
}

#ifdef __cplusplus
}
#endif