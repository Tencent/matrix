//
// Created by Yves on 2020-05-12.
//

#ifndef LIBWXPERF_JNI_MEMORYBENCHMARKTEST_H
#define LIBWXPERF_JNI_MEMORYBENCHMARKTEST_H

#define MEMORY_BENCHMARK_TAG "Wxperf.MemoryBenchmark"

#define ALLOC_TIMES 10000

void single_thread();

void cross_thread();

void concurrent_alloc();

#endif //LIBWXPERF_JNI_MEMORYBENCHMARKTEST_H
