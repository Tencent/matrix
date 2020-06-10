//
// Created by Yves on 2019-08-08.
//

#ifndef MEMINFO_MEMORYHOOK_H
#define MEMINFO_MEMORYHOOK_H

#include <cstddef>
#include <string>
#include <dlfcn.h>
#include "Log.h"
#include <jni.h>
#include <new>
#include "MemoryHookCXXFunctions.h"
#include "HookCommon.h"

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

DECLARE_HOOK_ORIG(int, munmap, void *__addr, size_t __size);

#ifdef __cplusplus
}
#endif

#endif //MEMINFO_MEMORYHOOK_H

