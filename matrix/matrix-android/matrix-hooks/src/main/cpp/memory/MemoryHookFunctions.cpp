/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created by Yves on 2020-04-28.
//

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <backtrace/BacktraceDefine.h>
#include "MemoryHookFunctions.h"
#include "MemoryHook.h"

#define ORIGINAL_LIB "libc.so"

#define DO_HOOK_ACQUIRE(p, size) \
    GET_CALLER_ADDR(caller); \
    on_alloc_memory(caller, p, size);

#define DO_HOOK_RELEASE(p) \
    on_free_memory(p)

DEFINE_HOOK_FUN(void *, malloc, size_t __byte_count) {
    CALL_ORIGIN_FUNC_RET(void*, p, malloc, __byte_count);
    LOGI(TAG, "+ malloc %p", p);
    DO_HOOK_ACQUIRE(p, __byte_count);
    return p;
}

DEFINE_HOOK_FUN(void *, calloc, size_t __item_count, size_t __item_size) {
    CALL_ORIGIN_FUNC_RET(void *, p, calloc, __item_count, __item_size);
    LOGI(TAG, "+ calloc %p", p);
    DO_HOOK_ACQUIRE(p, __item_count * __item_size);
    return p;
}

DEFINE_HOOK_FUN(void *, realloc, void *__ptr, size_t __byte_count) {
    CALL_ORIGIN_FUNC_RET(void *, p, realloc, __ptr, __byte_count);

    GET_CALLER_ADDR(caller);

    // If ptr is NULL, then the call is equivalent to malloc(size), for all values of size;
    // if size is equal to zero, and ptr is not NULL, then the call is equivalent to free(ptr).
    // Unless ptr is NULL, it must have been returned by an earlier call to malloc(), calloc() or realloc().
    // If the area pointed to was moved, a free(ptr) is done.
    if (!__ptr) { // malloc
        LOGI(TAG, "+ realloc1 %p", p);
        on_alloc_memory(caller, p, __byte_count);
        return p;
    } else if (!__byte_count) { // free
        on_free_memory(__ptr);
        return p;
    }

    LOGI(TAG, "+ realloc2 %p", p);

    // whatever has been moved or not, record anyway, because using realloc to shrink an allocation is allowed.
    // wtf whatever. if p == __ptr , just override meta
    if (p != __ptr) {
        on_free_memory(__ptr);
        on_alloc_memory(caller, p, __byte_count);
    } else {
        on_realloc_memory(caller, p, __byte_count);
    }

    return p;
}

DEFINE_HOOK_FUN(void *, memalign, size_t __alignment, size_t __byte_count) {
    CALL_ORIGIN_FUNC_RET(void *, p, memalign, __alignment, __byte_count);
    LOGI(TAG, "+ memalign %p", p);
    DO_HOOK_ACQUIRE(p, __byte_count);
    return p;
}

DEFINE_HOOK_FUN(int, posix_memalign, void** __memptr, size_t __alignment, size_t __size) {
    CALL_ORIGIN_FUNC_RET(int, ret, posix_memalign, __memptr, __alignment, __size);
    if (ret == 0) {
        LOGI(TAG, "+ posix_memalign %p", *__memptr);
        DO_HOOK_ACQUIRE(*__memptr, __size);
    }
    return ret;
}

DEFINE_HOOK_FUN(void, free, void *__ptr) {
    LOGI(TAG, "- free %p", __ptr);
    DO_HOOK_RELEASE(__ptr);
    CALL_ORIGIN_FUNC_VOID(free, __ptr);
}

#if defined(__USE_FILE_OFFSET64)
void*h_mmap(void* __addr, size_t __size, int __prot, int __flags, int __fd, off_t __offset) __RENAME(mmap64) {
    void * p = mmap(__addr, __size, __prot, __flags, __fd, __offset);
    GET_CALLER_ADDR(caller);
    on_mmap_memory(caller, p, __size);
    return p;
}
#else

DEFINE_HOOK_FUN(void *, mmap, void *__addr, size_t __size, int __prot, int __flags, int __fd,
                off_t              __offset) {
    CALL_ORIGIN_FUNC_RET(void *, p, mmap, __addr, __size, __prot, __flags, __fd, __offset);
    if (p == MAP_FAILED) {
        return p;// just return
    }
    LOGI(TAG, "+ mmap %p = mmap(addr=%p, size=%zu, prot=%d, flag=%d, fd=%d, offset=%ld", p, __addr, __size, __prot, __flags, __fd, __offset);
    GET_CALLER_ADDR(caller);
    on_mmap_memory(caller, p, __size);
    return p;
}


#endif

#if __ANDROID_API__ >= __ANDROID_API_L__

void *h_mmap64(void *__addr, size_t __size, int __prot, int __flags, int __fd,
               off64_t __offset) __INTRODUCED_IN(21) {
    void *p = mmap64(__addr, __size, __prot, __flags, __fd, __offset);
    if (p == MAP_FAILED) {
        return p;// just return
    }
    LOGI(TAG, "+ mmap64 %p = mmap64(addr=%p, size=%zu, prot=%d, flag=%d, fd=%d, offset=%lld", p, __addr, __size, __prot, __flags, __fd, (wechat_backtrace::llint_t)__offset);
    GET_CALLER_ADDR(caller);
    on_mmap_memory(caller, p, __size);
    return p;
}

#endif

DEFINE_HOOK_FUN(void *, mremap, void *__old_addr, size_t __old_size, size_t __new_size, int __flags,
                ...) {
    void *new_address = nullptr;
    if ((__flags & MREMAP_FIXED) != 0) {
        va_list ap;
        va_start(ap, __flags);
        new_address = va_arg(ap, void *);
        va_end(ap);
    }
    void *p           = mremap(__old_addr, __old_size, __new_size, __flags, new_address);
    if (p == MAP_FAILED) {
        return p; // just return
    }

    GET_CALLER_ADDR(caller);

    on_munmap_memory(__old_addr);
    LOGI(TAG, "+ mremap %p = mremap(addr=%p, __new_size=%zu", p, __old_addr, __new_size);
    on_mmap_memory(caller, p, __new_size);

    return p;
}

DEFINE_HOOK_FUN(int, munmap, void *__addr, size_t __size) {
    LOGI(TAG, "- munmap %p", __addr);
    on_munmap_memory(__addr);
    return munmap(__addr, __size);
}

DEFINE_HOOK_FUN(char*, strdup, const char *str) {
    CALL_ORIGIN_FUNC_RET(char *, p, strdup, str);
    LOGI(TAG, "+ strdup %p", (void *)p);
    DO_HOOK_ACQUIRE(p, sizeof(str));
    return p;
}

DEFINE_HOOK_FUN(char*, strndup, const char *str, size_t n) {
    CALL_ORIGIN_FUNC_RET(char *, p, strndup, str, n);
    LOGI(TAG, "+ strndup %p", (void *)p);
    DO_HOOK_ACQUIRE(p, sizeof(str) < n ? sizeof(str) : n);
    return p;
}

#undef ORIGINAL_LIB
#define ORIGINAL_LIB "libc++_shared.so"

#ifndef __LP64__

DEFINE_HOOK_FUN(void*, _Znwj, size_t size) {
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwj, size);
    LOGI(TAG, "+ _Znwj %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwjSt11align_val_t, size_t size, std::align_val_t align_val) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjSt11align_val_t, size, align_val);
    LOGI(TAG, "- _ZnwjSt11align_val_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwjSt11align_val_tRKSt9nothrow_t, size_t size,
                std::align_val_t align_val, std::nothrow_t const& nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    LOGI(TAG, "+ _ZnwjSt11align_val_tRKSt9nothrow_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwjRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwjRKSt9nothrow_t, size, nothrow);
    LOGI(TAG, "+ _ZnwjRKSt9nothrow_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _Znaj, size_t size) {
    CALL_ORIGIN_FUNC_RET(void*, p, _Znaj, size);
    LOGI(TAG, "+ _Znaj %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnajSt11align_val_t, size_t size, std::align_val_t align_val) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajSt11align_val_t, size, align_val);
    LOGI(TAG, "+ _ZnajSt11align_val_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnajSt11align_val_tRKSt9nothrow_t, size_t size,
                std::align_val_t align_val, std::nothrow_t const& nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    LOGI(TAG, "+ _ZnajSt11align_val_tRKSt9nothrow_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnajRKSt9nothrow_t, size_t size, std::nothrow_t const& nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnajRKSt9nothrow_t, size, nothrow);
    LOGI(TAG, "+ _ZnajRKSt9nothrow_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void, _ZdaPvj, void* ptr, size_t size) {
    LOGI(TAG, "- _ZdaPvj %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvj, ptr, size);
}

DEFINE_HOOK_FUN(void, _ZdaPvjSt11align_val_t, void* ptr, size_t size,
                std::align_val_t align_val) {
    LOGI(TAG, "- _ZdaPvjSt11align_val_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvjSt11align_val_t, ptr, size, align_val);
}

DEFINE_HOOK_FUN(void, _ZdlPvj, void* ptr, size_t size) {
    LOGI(TAG, "- _ZdlPvj %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvj, ptr, size);
}

DEFINE_HOOK_FUN(void, _ZdlPvjSt11align_val_t, void* ptr, size_t size,
                std::align_val_t align_val) {
    LOGI(TAG, "- _ZdlPvjSt11align_val_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvjSt11align_val_t, ptr, size, align_val);
}

#else

DEFINE_HOOK_FUN(void*, _Znwm, size_t size) {
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwm, size);
    LOGI(TAG, "+ _Znwm %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwmSt11align_val_t, size_t size, std::align_val_t align_val) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwmSt11align_val_t, size, align_val);
    LOGI(TAG, "+ _ZnwmSt11align_val_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwmSt11align_val_tRKSt9nothrow_t, size_t size,
                std::align_val_t                                  align_val,
                std::nothrow_t const                              &nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnwmSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    LOGI(TAG, "+ _ZnwmSt11align_val_tRKSt9nothrow_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnwmRKSt9nothrow_t, size_t size, std::nothrow_t const &nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _Znwm, size);
    LOGI(TAG, "+ _ZnwmRKSt9nothrow_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _Znam, size_t size) {
    CALL_ORIGIN_FUNC_RET(void*, p, _Znam, size);
    LOGI(TAG, "+ _Znam %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnamSt11align_val_t, size_t size, std::align_val_t align_val) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamSt11align_val_t, size, align_val);
    LOGI(TAG, "+ _ZnamSt11align_val_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnamSt11align_val_tRKSt9nothrow_t, size_t size,
                std::align_val_t                                  align_val,
                std::nothrow_t const                              &nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamSt11align_val_tRKSt9nothrow_t, size, align_val, nothrow);
    LOGI(TAG, "+ _ZnamSt11align_val_tRKSt9nothrow_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void*, _ZnamRKSt9nothrow_t, size_t size, std::nothrow_t const &nothrow) {
    CALL_ORIGIN_FUNC_RET(void*, p, _ZnamRKSt9nothrow_t, size, nothrow);
    LOGI(TAG, "+ _ZnamRKSt9nothrow_t %p", p);
    DO_HOOK_ACQUIRE(p, size);
    return p;
}

DEFINE_HOOK_FUN(void, _ZdlPvm, void *ptr, size_t size) {
    LOGI(TAG, "- _ZdlPvm %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvm, ptr, size);
}

DEFINE_HOOK_FUN(void, _ZdlPvmSt11align_val_t, void *ptr, size_t size,
                std::align_val_t                   align_val) {
    LOGI(TAG, "- _ZdlPvmSt11align_val_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvmSt11align_val_t, ptr, size, align_val);
}

DEFINE_HOOK_FUN(void, _ZdaPvm, void *ptr, size_t size) {
    LOGI(TAG, "- _ZdaPvm %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvm, ptr, size);
}

DEFINE_HOOK_FUN(void, _ZdaPvmSt11align_val_t, void *ptr, size_t size,
                std::align_val_t                   align_val) {
    LOGI(TAG, "- _ZdaPvmSt11align_val_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvmSt11align_val_t, ptr, size, align_val);
}

#endif

DEFINE_HOOK_FUN(void, _ZdlPv, void *p) {
    LOGI(TAG, "- _ZdlPv %p", p);
    DO_HOOK_RELEASE(p);
    CALL_ORIGIN_FUNC_VOID(_ZdlPv, p);
}

DEFINE_HOOK_FUN(void, _ZdlPvSt11align_val_t, void *ptr, std::align_val_t align_val) {
    LOGI(TAG, "- _ZdlPvSt11align_val_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvSt11align_val_t, ptr, align_val);
}

DEFINE_HOOK_FUN(void, _ZdlPvSt11align_val_tRKSt9nothrow_t, void *ptr,
                std::align_val_t                                align_val,
                std::nothrow_t const                            &nothrow) {
    LOGI(TAG, "- _ZdlPvSt11align_val_tRKSt9nothrow_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvSt11align_val_tRKSt9nothrow_t, ptr, align_val, nothrow);
}

DEFINE_HOOK_FUN(void, _ZdlPvRKSt9nothrow_t, void *ptr, std::nothrow_t const &nothrow) {
    LOGI(TAG, "- _ZdlPvRKSt9nothrow_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdlPvRKSt9nothrow_t, ptr, nothrow);
}

DEFINE_HOOK_FUN(void, _ZdaPv, void *ptr) {
    LOGI(TAG, "- _ZdaPv %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPv, ptr);
}

DEFINE_HOOK_FUN(void, _ZdaPvSt11align_val_t, void *ptr, std::align_val_t align_val) {
    LOGI(TAG, "- _ZdaPvSt11align_val_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvSt11align_val_t, ptr, align_val);
}

DEFINE_HOOK_FUN(void, _ZdaPvSt11align_val_tRKSt9nothrow_t, void *ptr,
                std::align_val_t                                align_val,
                std::nothrow_t const                            &nothrow) {
    LOGI(TAG, "- _ZdaPvSt11align_val_tRKSt9nothrow_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvSt11align_val_tRKSt9nothrow_t, ptr, align_val, nothrow);
}

DEFINE_HOOK_FUN(void, _ZdaPvRKSt9nothrow_t, void *ptr, std::nothrow_t const &nothrow) {
    LOGI(TAG, "- _ZdaPvRKSt9nothrow_t %p", ptr);
    DO_HOOK_RELEASE(ptr);
    CALL_ORIGIN_FUNC_VOID(_ZdaPvRKSt9nothrow_t, ptr, nothrow);
}

#undef ORIGINAL_LIB