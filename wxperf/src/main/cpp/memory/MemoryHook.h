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
#include "HookCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

// called by function definition

#define DO_HOOK_ACQUIRE(p, size) \
    GET_CALLER_ADDR(caller); \
    on_acquire_memory(caller, p, size);

#define DO_HOOK_RELEASE(p) \
    on_release_memory(p)

//#define DO_HOOK_RELEASE(sym, p, params...) \
//    GET_CALLER_ADDR(caller); \
//    on_release_memory(caller, p); \
//    ORIGINAL_FUNC_NAME(sym)(p, params);

DECLARE_HOOK_ORIG(void *, malloc, size_t __byte_count);

DECLARE_HOOK_ORIG(void *, calloc, size_t __item_count, size_t __item_size);

DECLARE_HOOK_ORIG(void *, realloc, void * __ptr, size_t __byte_count);

DECLARE_HOOK_ORIG(void, free, void *__ptr);

#if defined(__USE_FILE_OFFSET64)
// DECLARE_HOOK_ORIG not supports attrbute
void *h_mmap(void* __addr, size_t __size, int __prot, int __flags, int __fd, off_t __offset) __RENAME(mmap64);
#else
DECLARE_HOOK_ORIG(void *, mmap, void *__addr, size_t __size, int __prot, int __flags, int __fd, off_t __offset);
#endif

#if __ANDROID_API__ >= __ANDROID_API_L__
// DECLARE_HOOK_ORIG not supports attrbute
void *h_mmap64(void *__addr, size_t __size, int __prot, int __flags, int __fd,
               off64_t __offset) __INTRODUCED_IN(21);
#endif

DECLARE_HOOK_ORIG(void *, mremap, void*, size_t, size_t, int, ...)
//void* h_mremap(void* __old_addr, size_t __old_size, size_t __new_size, int __flags, ...);

DECLARE_HOOK_ORIG(int, munmap, void *__addr, size_t __size);
//int h_munmap(void *__addr, size_t __size);


void memory_hook_on_dlopen(const char *__file_name);

//DECLARE_HOOK_ORIG(void *, __loader_android_dlopen_ext, const char *filename,
//                  int flag,
//                  const void *extinfo,
//                  const void *caller_addr);

//JNIEXPORT void *h_dlopen(const char *filename,
//                         int flag,
//                         const void *extinfo,
//                         const void *caller_addr);

/******* not hook api below *******/

void dump(bool enable_mmap_hook = false, const char *path = "/sdcard/memory_hook.log");

void enable_stacktrace(bool);

void enable_group_by_size(bool);

void set_sample_size_range(size_t __min, size_t __max);

void set_sampling(double);

static uint64_t stacktrace_hash(uint64_t *);

#ifdef __cplusplus
}
#endif

#endif //MEMINFO_MEMORYHOOK_H

