//
// Created by Yves on 2019-08-08.
//

#ifndef MEMINFO_MEMORYHOOK_H
#define MEMINFO_MEMORYHOOK_H

#include <cstddef>
#include "log.h"
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GET_CALLER_ADDR(__caller_addr) \
    void * __caller_addr = __builtin_return_address(0);

JNIEXPORT void *h_malloc(size_t __byte_count);
JNIEXPORT void *h_calloc(size_t __item_count, size_t __item_size);
JNIEXPORT void *h_realloc(void *__ptr, size_t __byte_count);
JNIEXPORT void h_free(void *__ptr);

JNIEXPORT void init_if_necessary();

JNIEXPORT void *h_dlopen(const char *filename,
                         int flag,
                         const void *extinfo,
                         const void *caller_addr);

typedef void *(*ANDROID_DLOPEN)(const char *filename,
                                int flag,
                                const void *extinfo,
                                const void *caller_addr);

extern ANDROID_DLOPEN p_oldfun;

JNIEXPORT void dump();

JNIEXPORT void enableStacktrace(bool enable);

static uint64_t stacktrace_hash(uint64_t *);



#endif //MEMINFO_MEMORYHOOK_H

#ifdef __cplusplus
}
#endif