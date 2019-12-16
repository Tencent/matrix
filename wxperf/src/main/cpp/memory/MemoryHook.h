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
#include "MemoryHook_def.h"

#ifdef __cplusplus
extern "C" {
#endif

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

DECLARE_HOOK_ORIG(void *, dlopen, const char *filename,
                  int flag,
                  const void *extinfo,
                  const void *caller_addr);

//JNIEXPORT void *h_dlopen(const char *filename,
//                         int flag,
//                         const void *extinfo,
//                         const void *caller_addr);

/******* not hook api below *******/

void dump(bool enable_mmap_hook = false, const std::string path = "/sdcard/memory_hook.log");

void enableStacktrace(bool);

void enableGroupBySize(bool);

void setSampleSizeRange(size_t, size_t);

void setSampling(double);

static uint64_t stacktrace_hash(uint64_t *);

#endif //MEMINFO_MEMORYHOOK_H

#ifdef __cplusplus
}
#endif