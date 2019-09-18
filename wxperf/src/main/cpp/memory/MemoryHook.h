//
// Created by Yves on 2019-08-08.
//

#ifndef MEMINFO_MEMORYHOOK_H
#define MEMINFO_MEMORYHOOK_H

#include <cstddef>
#include <string>
#include "log.h"
#include <jni.h>
#include <new>
#include "MemoryHookCXX.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GET_CALLER_ADDR(__caller_addr) \
    void * __caller_addr = __builtin_return_address(0)

JNIEXPORT void *h_malloc(size_t __byte_count);
JNIEXPORT void *h_calloc(size_t __item_count, size_t __item_size);
JNIEXPORT void *h_realloc(void *__ptr, size_t __byte_count);
JNIEXPORT void h_free(void *__ptr);

JNIEXPORT void *h_dlopen(const char *filename,
                         int flag,
                         const void *extinfo,
                         const void *caller_addr);

JNIEXPORT void dump(std::string path = "/sdcard/memory_hook.log");

JNIEXPORT void enableStacktrace(bool);

JNIEXPORT void enableGroupBySize(bool);

JNIEXPORT void setSampleSizeRange(size_t, size_t);

JNIEXPORT void setSampling(double);

static uint64_t stacktrace_hash(uint64_t *);

typedef void *(*ANDROID_DLOPEN)(const char *filename,
                                int flag,
                                const void *extinfo,
                                const void *caller_addr);

extern ANDROID_DLOPEN orig_dlopen;

#endif //MEMINFO_MEMORYHOOK_H

#ifdef __cplusplus
}
#endif