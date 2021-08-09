//
// Created by Yves on 2020-05-12.
//

#include <malloc.h>
#include "MemoryBenchmarkTest.h"
#include "BenchMarkCommon.h"

void single_thread() {

    NanoSeconds_Start(nano);

    void * ptrs[ALLOC_TIMES];

    for (int i = 0; i < ALLOC_TIMES; ++i) {
        ptrs[i] = malloc(512);
    }
    NanoSeconds_End(MEMORY_BENCHMARK_TAG, nano);

    NanoSeconds_Start(release);

    for (int i = 0; i < ALLOC_TIMES; ++i) {
        if (ptrs[i]) {
            free(ptrs[i]);
        }
    }

    NanoSeconds_End(MEMORY_BENCHMARK_TAG, release);

}