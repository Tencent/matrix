// ported from https://github.com/mjansson/rpmalloc-benchmark

#ifdef __cplusplus
extern "C" {
#endif

#include "BenchMarkCommon.h"
#include <cstring>
#include <cstdlib>

#ifndef __APPLE__
#  include <malloc.h>
#endif

int
benchmark_initialize() {
    return 0;
}

int
benchmark_finalize(void) {
    return 0;
}

int
benchmark_thread_initialize(void) {
    return 0;
}

int
benchmark_thread_finalize(void) {
    return 0;
}

void*
benchmark_malloc(size_t alignment, size_t size) {
    /*
#ifdef _WIN32
    return _aligned_malloc(size, alignment ? alignment : 4);
#elif defined(__APPLE__)
    if (alignment) {
        void* ptr = 0;
        posix_memalign(&ptr, alignment, size);
        return ptr;
    }
    return malloc(size);
#else
    return alignment ? memalign(alignment, size) : malloc(size);
#endif
    */
    return malloc(size);
}

extern void
benchmark_free(void* ptr) {
    /*
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
    */
    free(ptr);
}

const char*
benchmark_name(void) {
    return "crt";
}

void
benchmark_thread_collect(void) {
}


#ifdef __cplusplus
}
#endif