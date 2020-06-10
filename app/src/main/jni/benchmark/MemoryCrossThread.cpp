//
// Created by Yves on 2020-05-12.
//

#include <malloc.h>
#include <pthread.h>
#include <atomic>
#include "MemoryBenchmarkTest.h"
#include "BenchMarkCommon.h"

void * ptrs[ALLOC_TIMES];

std::atomic_bool barrier;

void *free_thread(void *) {

    barrier.load(std::memory_order_acquire);

    for (int i = 0; i < ALLOC_TIMES; ++i) {
        if (ptrs[i]) {
            free(ptrs[i]);
        }
    }

    return nullptr;
}

void cross_thread() {

    NanoSeconds_Start(nano);

    for (int i = 0; i < ALLOC_TIMES; ++i) {
        ptrs[i] = malloc(512);
    }

    barrier.store(true, std::memory_order_release);

    pthread_t pthread;
    pthread_create(&pthread, NULL, free_thread, NULL);
    pthread_join(pthread, NULL);

    NanoSeconds_End(MEMORY_BENCHMARK_TAG, nano);
}