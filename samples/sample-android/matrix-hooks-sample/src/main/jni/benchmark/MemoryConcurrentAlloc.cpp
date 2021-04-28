//
// Created by Yves on 2020-05-12.
//

#include <malloc.h>
#include <pthread.h>
#include <unistd.h>
#include "MemoryBenchmarkTest.h"
#include "BenchMarkCommon.h"

#define THREAD_ALLOC_TIMES 2048
#define THREAD_COUNT 50

#define CON_ALLOC_TAG MEMORY_BENCHMARK_TAG
#define CON_FREE_TAG MEMORY_BENCHMARK_TAG

void *thread_routine(void *) {

    void *ptrs[THREAD_ALLOC_TIMES];

    for (int i = 0; i < THREAD_ALLOC_TIMES; ++i) {
        if (i < 2) {
            NanoSeconds_Start(alloc);
            ptrs[i] = malloc(64);
            NanoSeconds_End(CON_ALLOC_TAG, alloc);
        }

        ptrs[i] = malloc(64);
    }



//    NanoSeconds_Start(dalloc);

    for (int i = 0; i < THREAD_ALLOC_TIMES; ++i) {
        if (ptrs[i]) {
            free(ptrs[i]);
        }
    }

//    NanoSeconds_End(CON_FREE_TAG, dalloc);

    return nullptr;
}

void concurrent_alloc() {

    pthread_t pthreads[THREAD_COUNT];

//    for (int j = 0; j < 100; ++j) {


        for (int i = 0; i < THREAD_COUNT; ++i) {
            pthread_create(&pthreads[i], NULL, thread_routine, NULL);
        }

        for (int i = 0; i < THREAD_COUNT; ++i) {
            pthread_join(pthreads[i], NULL);
        }

//    }

}