//
// Created by tomystang on 2020/10/16.
//
#include <cstdlib>
#include <cinttypes>
#include <cerrno>
#include <Options.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <pthread.h>
#include <HookCommon.h>
#include <SoLoadMonitor.h>
#include "Auxiliary.h"
#include "Allocation.h"
#include "Hook.h"
#include "Interception.h"
#include "Log.h"
#include "Memory.h"

using namespace memguard;

#define LOG_TAG "MemGuard.Interception"

#if defined(__LP64__)
#define DEFAULT_PTR_ALIGN 16
#else
#define DEFAULT_PTR_ALIGN 8
#endif

static void* sLibCHandle = nullptr;
static void* sSelfLibHandle = nullptr;

static void* HandleMAlloc(size_t size);
static void* HandleCAlloc(size_t elem_count, size_t elem_size);
static void* HandleReAlloc(void* ptr, size_t size);
static void HandleFree(void* ptr);
static void* HandleMemAlign(size_t alignment, size_t byte_count);
static int HandlePosixMemAlign(void** ptr, size_t alignment, size_t size);
static void* HandleAlignedAlloc(size_t alignment, size_t size);
static void* HandleVAlloc(size_t size);
static void* HandlePVAlloc(size_t size);
static char* HandleStrDup(const char* str);
static char* HandleStrNDup(const char* str, size_t n);
static size_t HandleMAllocUsableSize(const void* ptr);
static void HandleFree(void* ptr);
static void* HandleNew(size_t size);
static void* HandleNewNoThrow(size_t size, const std::nothrow_t&);
static void* HandleNewAligned(size_t size, size_t align);
static void* HandleNewAlignedNoThrow(size_t size, size_t align, const std::nothrow_t&);
static void* HandleNewArr(size_t size);
static void* HandleNewArrNoThrow(size_t size, const std::nothrow_t&);
static void* HandleNewArrAligned(size_t size, size_t align);
static void* HandleNewArrAlignedNoThrow(size_t size, size_t align, const std::nothrow_t&);
static void HandleDelete(void* ptr);
static void HandleDeleteNoThrow(void* ptr, const std::nothrow_t&);
static void HandleDeleteAligned(void* ptr, size_t align);
static void HandleDeleteAlignedNoThrow(void* ptr, size_t align, const std::nothrow_t&);
static void HandleDeleteSize(void* ptr, unsigned int size);
static void HandleDeleteSizeAligned(void* ptr, unsigned int size, size_t align);
static void HandleDeleteArr(void* ptr);
static void HandleDeleteArrNoThrow(void* ptr, const std::nothrow_t&);
static void HandleDeleteArrAligned(void* ptr, size_t align);
static void HandleDeleteArrAlignedNoThrow(void* ptr, size_t align, const std::nothrow_t&);
static void HandleDeleteArrSize(void* ptr, unsigned int size);
static void HandleDeleteArrSizeAligned(void* ptr, unsigned int size, size_t align);

#ifdef __LP64__
    #define SYM_NEW _Znwm
    #define SYM_NEW_NOTHROW _ZnwmRKSt9nothrow_t
    #define SYM_NEW_ALIGNED _ZnwmSt11align_val_t
    #define SYM_NEW_ALIGNED_NOTHROW _ZnwmSt11align_val_tRKSt9nothrow_t
    #define SYM_NEW_ARR _Znam
    #define SYM_NEW_ARR_NOTHROW _ZnamRKSt9nothrow_t
    #define SYM_NEW_ARR_ALIGNED _ZnamSt11align_val_t
    #define SYM_NEW_ARR_ALIGNED_NOTHROW _ZnamSt11align_val_tRKSt9nothrow_t
    #define SYM_DELETE_SIZE _ZdlPvm
    #define SYM_DELETE_SIZE_ALIGNED _ZdlPvmSt11align_val_t
    #define SYM_DELETE_ARR_SIZE _ZdaPvm
    #define SYM_DELETE_ARR_SIZE_ALIGNED _ZdaPvmSt11align_val_t
#else
    #define SYM_NEW _Znwj
    #define SYM_NEW_NOTHROW _ZnwjRKSt9nothrow_t
    #define SYM_NEW_ALIGNED _ZnwjSt11align_val_t
    #define SYM_NEW_ALIGNED_NOTHROW _ZnwjSt11align_val_tRKSt9nothrow_t
    #define SYM_NEW_ARR _Znaj
    #define SYM_NEW_ARR_NOTHROW _ZnajRKSt9nothrow_t
    #define SYM_NEW_ARR_ALIGNED _ZnajSt11align_val_t
    #define SYM_NEW_ARR_ALIGNED_NOTHROW _ZnajSt11align_val_tRKSt9nothrow_t
    #define SYM_DELETE_SIZE _ZdlPvj
    #define SYM_DELETE_SIZE_ALIGNED _ZdlPvjSt11align_val_t
    #define SYM_DELETE_ARR_SIZE _ZdaPvj
    #define SYM_DELETE_ARR_SIZE_ALIGNED _ZdaPvjSt11align_val_t
#endif

#define SYM_DELETE _ZdlPv
#define SYM_DELETE_NOTHROW _ZdlPvRKSt9nothrow_t
#define SYM_DELETE_ALIGNED _ZdlPvSt11align_val_t
#define SYM_DELETE_ALIGNED_NOTHROW _ZdlPvSt11align_val_tRKSt9nothrow_t
#define SYM_DELETE_ARR _ZdaPv
#define SYM_DELETE_ARR_NOTHROW _ZdaPvRKSt9nothrow_t
#define SYM_DELETE_ARR_ALIGNED _ZdaPvSt11align_val_t
#define SYM_DELETE_ARR_ALIGNED_NOTHROW _ZdaPvSt11align_val_tRKSt9nothrow_t

#define _X_6(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle) \
  _X_5(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle)
#define _X_5(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle) \
  _X_4(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle)
#define _X_4(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle) \
  _X_3(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle)
#define _X_3(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle) \
  _X_2(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle)
#define _X_2(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle) \
  _X_1(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle)
#define _X_1(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle) \
  X(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle)

#define ENUM_C_ALLOC_FUNCTIONS(target_lib_pattern) \
  _X_6(target_lib_pattern, void*, malloc, (size_t), HandleMAlloc, sLibCHandle) \
  _X_6(target_lib_pattern, void*, calloc, (size_t, size_t), HandleCAlloc, sLibCHandle) \
  _X_6(target_lib_pattern, void*, realloc, (void*, size_t), HandleReAlloc, sLibCHandle) \
  _X_6(target_lib_pattern, void*, memalign, (size_t, size_t), HandleMemAlign, sLibCHandle) \
  _X_6(target_lib_pattern, int, posix_memalign, (void**, size_t, size_t), HandlePosixMemAlign, sLibCHandle) \
  _X_6(target_lib_pattern, void*, aligned_alloc, (size_t, size_t), HandleAlignedAlloc, sLibCHandle) \
  _X_6(target_lib_pattern, char*, strdup, (const char*), HandleStrDup, sLibCHandle) \
  _X_6(target_lib_pattern, char*, strndup, (const char*, size_t), HandleStrNDup, sLibCHandle)

#define ENUM_C_DEALLOC_AND_OTHER_FUNCTIONS(target_lib_pattern) \
  _X_6(target_lib_pattern, size_t, malloc_usable_size, (const void*), HandleMAllocUsableSize, sLibCHandle) \
  _X_6(target_lib_pattern, void, free, (void*), HandleFree, sLibCHandle)

#define ENUM_CPP_ALLOC_FUNCTIONS(target_lib_pattern) \
  _X_6(target_lib_pattern, void*, SYM_NEW, (size_t), HandleNew, sSelfLibHandle) \
  _X_6(target_lib_pattern, void*, SYM_NEW_NOTHROW, (size_t, const std::nothrow_t&), HandleNewNoThrow, sSelfLibHandle) \
  _X_6(target_lib_pattern, void*, SYM_NEW_ALIGNED, (size_t, size_t), HandleNewAligned, sSelfLibHandle) \
  _X_6(target_lib_pattern, void*, SYM_NEW_ALIGNED_NOTHROW, (size_t, size_t, const std::nothrow_t&), HandleNewAlignedNoThrow, sSelfLibHandle) \
  _X_6(target_lib_pattern, void*, SYM_NEW_ARR, (size_t), HandleNewArr, sSelfLibHandle) \
  _X_6(target_lib_pattern, void*, SYM_NEW_ARR_NOTHROW, (size_t, const std::nothrow_t&), HandleNewArrNoThrow, sSelfLibHandle) \
  _X_6(target_lib_pattern, void*, SYM_NEW_ARR_ALIGNED, (size_t, size_t), HandleNewArrAligned, sSelfLibHandle) \
  _X_6(target_lib_pattern, void*, SYM_NEW_ARR_ALIGNED_NOTHROW, (size_t, size_t, const std::nothrow_t&), HandleNewArrAlignedNoThrow, sSelfLibHandle)

#define ENUM_CPP_DEALLOC_FUNCTIONS(target_lib_pattern) \
  _X_6(target_lib_pattern, void, SYM_DELETE, (void*), HandleDelete, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_NOTHROW, (void*, const std::nothrow_t&), HandleDeleteNoThrow, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_ALIGNED, (void*, size_t), HandleDeleteAligned, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_ALIGNED_NOTHROW, (void*, size_t, const std::nothrow_t&), HandleDeleteAlignedNoThrow, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_ARR, (void*), HandleDeleteArr, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_ARR_NOTHROW, (void*, const std::nothrow_t&), HandleDeleteArrNoThrow, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_ARR_ALIGNED, (void*, size_t), HandleDeleteArrAligned, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_ARR_ALIGNED_NOTHROW, (void*, size_t, const std::nothrow_t&), HandleDeleteArrAlignedNoThrow, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_SIZE, (void*, unsigned int), HandleDeleteSize, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_SIZE_ALIGNED, (void*, unsigned int, size_t), HandleDeleteSizeAligned, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_ARR_SIZE, (void*, unsigned int), HandleDeleteArrSize, sSelfLibHandle) \
  _X_6(target_lib_pattern, void, SYM_DELETE_ARR_SIZE_ALIGNED, (void*, unsigned int, size_t), HandleDeleteArrSizeAligned, sSelfLibHandle)

static struct {
  #define X(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle) \
    ret_type (*orig_##sym) args = nullptr;

  ENUM_C_ALLOC_FUNCTIONS(_)
  ENUM_C_DEALLOC_AND_OTHER_FUNCTIONS(_)
  ENUM_CPP_ALLOC_FUNCTIONS(_)
  ENUM_CPP_DEALLOC_FUNCTIONS(_)

  #undef X
} sOriginalFunctions;

#define ORIGINAL_FUNCTION(sym) _ORIGINAL_FUNCTION_1(sym)
#define _ORIGINAL_FUNCTION_1(sym) sOriginalFunctions.orig_##sym

static void* HandleMAlloc(size_t size) {
    void* res = allocation::Allocate(size);
    if (res == nullptr) {
        res = ORIGINAL_FUNCTION(malloc)(size);
        LOGD(LOG_TAG, "HandleMAlloc(%" PRIu32 ") = %p, skipped.", size, res);
    } else {
        LOGD(LOG_TAG, "HandleMAlloc(%" PRIu32 ") = %p, guarded.", size, res);
        errno = 0;
    }
    return res;
}

static void* HandleCAlloc(size_t elem_count, size_t elem_size) {
    void* res = allocation::Allocate(elem_count * elem_size);
    if (res == nullptr) {
        res = ORIGINAL_FUNCTION(calloc)(elem_count, elem_size);
        LOGD(LOG_TAG, "HandleCAlloc(%" PRIu32 ",%" PRIu32 ") = %p, skipped.", elem_count, elem_size, res);
    } else {
        memset(res, 0, elem_count * elem_size);
        errno = 0;
        LOGD(LOG_TAG, "HandleCAlloc(%" PRIu32 ",%" PRIu32 ") = %p, guarded.", elem_count, elem_size, res);
    }
    return res;
}

static void* HandleReAlloc(void* ptr, size_t size) {
    if (ptr == nullptr) {
        return ORIGINAL_FUNCTION(malloc)(size);
    }
    if (size == 0) {
        HandleFree(ptr);
        errno = 0;
        return nullptr;
    }
    if (LIKELY(!allocation::IsAllocatedByThisAllocator(ptr))) {
        void* res = ORIGINAL_FUNCTION(realloc)(ptr, size);
        LOGD(LOG_TAG, "HandleReAlloc(%p,%" PRIu32 ") = %p, skipped.", ptr, size, res);
        errno = 0;
        return res;
    }
    void* newPtr = HandleMAlloc(size);
    if (UNLIKELY(newPtr == nullptr)) {
        LOGD(LOG_TAG, "HandleReAlloc(%p,%" PRIu32 ") = %p, failure.", ptr, size, newPtr);
        errno = ENOMEM;
        return nullptr;
    }
    size_t oldSize = allocation::GetAllocatedSize(ptr);
    memcpy(newPtr, ptr, (oldSize <= size ? oldSize : size));
    allocation::Free(ptr);
    LOGD(LOG_TAG, "HandleReAlloc(%p,%" PRIu32 ") = %p, moved, guarded.", ptr, size, newPtr);
    errno = 0;
    return newPtr;
}

static void* HandleMemAlign(size_t alignment, size_t byte_count) {
    if (UNLIKELY(!IsPowOf2(alignment))) {
        errno = EINVAL;
        return nullptr;
    }
    void* res = allocation::AlignedAllocate(byte_count, alignment);
    if (res != nullptr) {
        errno = 0;
        LOGD(LOG_TAG, "HandleMemAlign(%" PRIu32 ",%" PRIu32 ") = %p, guarded.", alignment, byte_count, res);
        return res;
    } else {
        res = ORIGINAL_FUNCTION(memalign)(alignment, byte_count);
        LOGD(LOG_TAG, "HandleMemAlign(%" PRIu32 ",%" PRIu32 ") = %p, skipped.", alignment, byte_count, res);
        return res;
    }
}

static int HandlePosixMemAlign(void** ptr, size_t alignment, size_t size) {
    if (UNLIKELY((alignment & (sizeof(void*) - 1)) != 0)) {
        return EINVAL;
    }
    void* res = allocation::AlignedAllocate(size, alignment);
    if (res != nullptr) {
        LOGD(LOG_TAG, "HandlePosixMemAlign(%p,%" PRIu32 ",%" PRIu32 ") = 0 (ptr_res:%p), guarded.",
             ptr, alignment, size, res);
        *ptr = res;
        errno = 0;
        return 0;
    } else {
        int originalFnRet = ORIGINAL_FUNCTION(posix_memalign)(ptr, alignment, size);
        LOGD(LOG_TAG, "HandlePosixMemAlign(%p,%" PRIu32 ",%" PRIu32 ") = %d (ptr_res:%p), skipped.",
             ptr, alignment, size, originalFnRet, *ptr);
        return originalFnRet;
    }
}

static void* HandleAlignedAlloc(size_t alignment, size_t size) {
    if (UNLIKELY(size % alignment != 0)) {
        errno = EINVAL;
        return nullptr;
    }
    return HandleMemAlign(alignment, size);
}

static char* HandleStrDup(const char* str) {
    size_t len = strlen(str);
    char* buf = (char*) allocation::Allocate(len + 1);
    if (buf != nullptr) {
        ::memcpy(buf, str, len + 1);
        errno = 0;
        LOGD(LOG_TAG, "HandleStrDup(%p) = %p, guarded.", str, buf);
    } else {
        buf = ORIGINAL_FUNCTION(strdup)(str);
        LOGD(LOG_TAG, "HandleStrDup(%p) = %p, skipped.", str, buf);
    }
    return buf;
}

static char* HandleStrNDup(const char* str, size_t n) {
    size_t len = strlen(str);
    size_t dupLen = (n <= len ? n : len);
    char* buf = (char*) allocation::Allocate(dupLen + 1);
    if (buf != nullptr) {
        ::memcpy(buf, str, dupLen);
        buf[dupLen] = '\0';
        errno = 0;
        LOGD(LOG_TAG, "HandleStrNDup(%p, %" PRIu32 ") = %p, guarded.", str, n, buf);
    } else {
        buf = ORIGINAL_FUNCTION(strndup)(str, n);
        LOGD(LOG_TAG, "HandleStrNDup(%p, %" PRIu32 ") = %p, skipped.", str, n, buf);
    }
    return buf;
}

static size_t HandleMAllocUsableSize(const void* ptr) {
    size_t res = 0;
    if (allocation::IsAllocatedByThisAllocator((void*) ptr)) {
        res = allocation::GetAllocatedSize((void*) ptr);
        LOGD(LOG_TAG, "HandleMAllocUsableSize(%p) = %" PRIu32 ", guarded.", ptr, res);
    } else {
        res = ORIGINAL_FUNCTION(malloc_usable_size)(ptr);
        LOGD(LOG_TAG, "HandleMAllocUsableSize(%p) = %" PRIu32 ", skipped.", ptr, res);
    }
    return res;
}

static void HandleFree(void* ptr) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(free)(ptr);
        LOGD(LOG_TAG, "HandleFree(%p), skipped.", ptr);
    } else {
        errno = 0;
        LOGD(LOG_TAG, "HandleFree(%p), recycled.", ptr);
    }
}

static void* HandleNew(size_t size) {
    void* result = allocation::AlignedAllocate(size, DEFAULT_PTR_ALIGN);
    if (result != nullptr) {
        LOGD(LOG_TAG, "HandleNew(%" PRIu32 ") = %p, guarded.", size, result);
        return result;
    } else {
        LOGD(LOG_TAG, "HandleNew(%" PRIu32 ") = %p, skipped.", size, result);
        return ORIGINAL_FUNCTION(SYM_NEW)(size);
    }
}

static void* HandleNewNoThrow(size_t size, const std::nothrow_t&) {
    void* result = allocation::AlignedAllocate(size, DEFAULT_PTR_ALIGN);
    if (result != nullptr) {
        LOGD(LOG_TAG, "HandleNewNoThrow(%" PRIu32 ") = %p, guarded.", size, result);
        return result;
    } else {
        LOGD(LOG_TAG, "HandleNewNoThrow(%" PRIu32 ") = %p, skipped.", size, result);
        return ORIGINAL_FUNCTION(SYM_NEW_NOTHROW)(size, std::nothrow);
    }
}

static void* HandleNewAligned(size_t size, size_t align) {
    void* result = allocation::AlignedAllocate(size, align);
    if (result != nullptr) {
        LOGD(LOG_TAG, "HandleNewAligned(%" PRIu32 ", %" PRIu32 ") = %p, guarded.", size, align, result);
        return result;
    } else {
        LOGD(LOG_TAG, "HandleNewAligned(%" PRIu32 ", %" PRIu32 ") = %p, skipped.", size, align, result);
        return ORIGINAL_FUNCTION(SYM_NEW_ALIGNED)(size, align);
    }
}

static void* HandleNewAlignedNoThrow(size_t size, size_t align, const std::nothrow_t&) {
    void* result = allocation::AlignedAllocate(size, align);
    if (result != nullptr) {
        LOGD(LOG_TAG, "HandleNewAlignedNoThrow(%" PRIu32 ", %" PRIu32 ") = %p, guarded.", size, align, result);
        return result;
    } else {
        LOGD(LOG_TAG, "HandleNewAlignedNoThrow(%" PRIu32 ", %" PRIu32 ") = %p, skipped.", size, align, result);
        return ORIGINAL_FUNCTION(SYM_NEW_ALIGNED_NOTHROW)(size, align, std::nothrow);
    }
}

static void* HandleNewArr(size_t size) {
    void* result = allocation::AlignedAllocate(size, DEFAULT_PTR_ALIGN);
    if (result != nullptr) {
        LOGD(LOG_TAG, "HandleNewArr(%" PRIu32 ") = %p, guarded.", size, result);
        return result;
    } else {
        LOGD(LOG_TAG, "HandleNewArr(%" PRIu32 ") = %p, skipped.", size, result);
        return ORIGINAL_FUNCTION(SYM_NEW_ARR)(size);
    }
}

static void* HandleNewArrNoThrow(size_t size, const std::nothrow_t&) {
    void* result = allocation::AlignedAllocate(size, DEFAULT_PTR_ALIGN);
    if (result != nullptr) {
        LOGD(LOG_TAG, "HandleNewArrNoThrow(%" PRIu32 ") = %p, guarded.", size, result);
        return result;
    } else {
        LOGD(LOG_TAG, "HandleNewArrNoThrow(%" PRIu32 ") = %p, skipped.", size, result);
        return ORIGINAL_FUNCTION(SYM_NEW_ARR_NOTHROW)(size, std::nothrow);
    }
}

static void* HandleNewArrAligned(size_t size, size_t align) {
    void* result = allocation::AlignedAllocate(size, align);
    if (result != nullptr) {
        LOGD(LOG_TAG, "HandleNewArrAligned(%" PRIu32 ", %" PRIu32 ") = %p, guarded.", size, align, result);
        return result;
    } else {
        LOGD(LOG_TAG, "HandleNewArrAligned(%" PRIu32 ", %" PRIu32 ") = %p, guarded.", size, align, result);
        return ORIGINAL_FUNCTION(SYM_NEW_ARR_ALIGNED)(size, align);
    }
}

static void* HandleNewArrAlignedNoThrow(size_t size, size_t align, const std::nothrow_t&) {
    void* result = allocation::AlignedAllocate(size, align);
    if (result != nullptr) {
        LOGD(LOG_TAG, "HandleNewArrAlignedNoThrow(%" PRIu32 ", %" PRIu32 ") = %p, guarded.", size, align, result);
        return result;
    } else {
        LOGD(LOG_TAG, "HandleNewArrAlignedNoThrow(%" PRIu32 ", %" PRIu32 ") = %p, guarded.", size, align, result);
        return ORIGINAL_FUNCTION(SYM_NEW_ARR_ALIGNED_NOTHROW)(size, align, std::nothrow);
    }
}

static void HandleDelete(void* ptr) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE)(ptr);
        LOGD(LOG_TAG, "HandleDelete(%p), skipped.", ptr);
    } else {
        LOGD(LOG_TAG, "HandleDelete(%p), recycled.", ptr);
    }
}

static void HandleDeleteNoThrow(void* ptr, const std::nothrow_t&) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_NOTHROW)(ptr, std::nothrow);
        LOGD(LOG_TAG, "HandleDeleteNoThrow(%p), skipped.", ptr);
    } else {
        LOGD(LOG_TAG, "HandleDeleteNoThrow(%p), recycled.", ptr);
    }
}

static void HandleDeleteAligned(void* ptr, size_t align) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_ALIGNED)(ptr, align);
        LOGD(LOG_TAG, "HandleDeleteAligned(%p, %" PRIu32 "), skipped.", ptr, align);
    } else {
        LOGD(LOG_TAG, "HandleDeleteAligned(%p, %" PRIu32 "), recycled.", ptr, align);
    }
}

static void HandleDeleteAlignedNoThrow(void* ptr, size_t align, const std::nothrow_t&) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_ALIGNED_NOTHROW)(ptr, align, std::nothrow);
        LOGD(LOG_TAG, "HandleDeleteAlignedNoThrow(%p, %" PRIu32 "), skipped.", ptr, align);
    } else {
        LOGD(LOG_TAG, "HandleDeleteAlignedNoThrow(%p, %" PRIu32 "), recycled.", ptr, align);
    }
}

static void HandleDeleteSize(void* ptr, unsigned int size) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_SIZE)(ptr, size);
        LOGD(LOG_TAG, "HandleDeleteSize(%p, %" PRIu32 "), skipped.", ptr, size);
    } else {
        LOGD(LOG_TAG, "HandleDeleteSize(%p, %" PRIu32 "), recycled.", ptr, size);
    }
}

static void HandleDeleteSizeAligned(void* ptr, unsigned int size, size_t align) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_SIZE_ALIGNED)(ptr, size, align);
        LOGD(LOG_TAG, "HandleDeleteSizeAligned(%p, %" PRIu32 ", %" PRIu32 "), skipped.", ptr, size, align);
    } else {
        LOGD(LOG_TAG, "HandleDeleteSizeAligned(%p, %" PRIu32 ", %" PRIu32 "), recycled.", ptr, size, align);
    }
}

static void HandleDeleteArr(void* ptr) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_ARR)(ptr);
        LOGD(LOG_TAG, "HandleDeleteArr(%p), skipped.", ptr);
    } else {
        LOGD(LOG_TAG, "HandleDeleteArr(%p), recycled.", ptr);
    }
}

static void HandleDeleteArrNoThrow(void* ptr, const std::nothrow_t&) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_ARR_NOTHROW)(ptr, std::nothrow);
        LOGD(LOG_TAG, "HandleDeleteArrNoThrow(%p), skipped.", ptr);
    } else {
        LOGD(LOG_TAG, "HandleDeleteArrNoThrow(%p), recycled.", ptr);
    }
}

static void HandleDeleteArrAligned(void* ptr, size_t align) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_ARR_ALIGNED)(ptr,  align);
        LOGD(LOG_TAG, "HandleDeleteArrAligned(%p, %" PRIu32 "), skipped.", ptr, align);
    } else {
        LOGD(LOG_TAG, "HandleDeleteArrAligned(%p, %" PRIu32 "), recycled.", ptr, align);
    }
}

static void HandleDeleteArrAlignedNoThrow(void* ptr, size_t align, const std::nothrow_t&) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_ARR_ALIGNED_NOTHROW)(ptr, align, std::nothrow);
        LOGD(LOG_TAG, "HandleDeleteArrAlignedNoThrow(%p, %" PRIu32 "), skipped.", ptr, align);
    } else {
        LOGD(LOG_TAG, "HandleDeleteArrAlignedNoThrow(%p, %" PRIu32 "), recycled.", ptr, align);
    }
}

static void HandleDeleteArrSize(void* ptr, unsigned int size) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_ARR_SIZE)(ptr, size);
        LOGD(LOG_TAG, "HandleDeleteArrSize(%p, %" PRIu32 "), skipped.", ptr, size);
    } else {
        LOGD(LOG_TAG, "HandleDeleteArrSize(%p, %" PRIu32 "), recycled.", ptr, size);
    }
}

static void HandleDeleteArrSizeAligned(void* ptr, unsigned int size, size_t align) {
    if (!allocation::Free(ptr)) {
        ORIGINAL_FUNCTION(SYM_DELETE_ARR_SIZE_ALIGNED)(ptr, size, align);
        LOGD(LOG_TAG, "HandleDeleteArrSizeAligned(%p, %" PRIu32 ", %" PRIu32 "), skipped.", ptr, size, align);
    } else {
        LOGD(LOG_TAG, "HandleDeleteArrSizeAligned(%p, %" PRIu32 ", %" PRIu32 "), recycled.", ptr, size, align);
    }
}

static bool InitializeOriginalFunctions() {
  sLibCHandle = ::dlopen("libc.so", RTLD_NOW);
  if (sLibCHandle == nullptr) {
      LOGE(LOG_TAG, "Fail to get handle of libc.so");
      return false;
  }

  sSelfLibHandle = ::dlopen(nullptr, RTLD_NOW);
  if (sSelfLibHandle == nullptr) {
      LOGE(LOG_TAG, "Fail to get handle of myself.");
      return false;
  }

  #define X(target_lib_pattern, ret_type, sym, args, handler, def_lib_handle) \
    do { \
      void* hLib = (def_lib_handle); \
      ORIGINAL_FUNCTION(sym) = (ret_type (*) args) dlsym(hLib, #sym); \
      if (UNLIKELY(ORIGINAL_FUNCTION(sym) == nullptr)) { \
        LOGE(LOG_TAG, "Fail to get address of symbol: %s.", #sym); \
        return false; \
      } \
    } while (false);

    ENUM_C_ALLOC_FUNCTIONS(_)
    ENUM_C_DEALLOC_AND_OTHER_FUNCTIONS(_)
    ENUM_CPP_ALLOC_FUNCTIONS(_)
    ENUM_CPP_DEALLOC_FUNCTIONS(_)

  #undef X

  return true;
}

// static void* (*sOriginalLoaderDlOpen)(const char*, int, const void*) = nullptr;
// static void* HandleLoaderDlOpen(const char* path, int flag, const void* caller_addr) {
//     void* result = sOriginalLoaderDlOpen(path, flag, caller_addr);
//     if (!UpdateHook()) {
//         LOGE(LOG_TAG, "Fail to update hook when load '%s' with flag '%d'", path, flag);
//     }
//     return result;
// }
//
// static void* (*sOriginalAndroidDlOpenExt)(const char*, int, const void*, const void*) = nullptr;
// static void* HandleAndroidDlOpenExt(const char* path, int flag, const void* extinfo, const void* caller_addr) {
//     void* result = sOriginalAndroidDlOpenExt(path, flag, extinfo, caller_addr);
//     if (!UpdateHook()) {
//         LOGE(LOG_TAG, "Fail to update hook when load '%s' with flag '%d'", path, flag);
//     }
//     return result;
// }

#define INTERCEPT_DLOPEN(pattern, failure_ret_val) \
  do { \
    if (!DoHook(pattern, "__loader_dlopen", (void *) HandleLoaderDlOpen, (void **) &sOriginalLoaderDlOpen)) { \
      return (failure_ret_val); \
    } \
    if (!DoHook(pattern, "android_dlopen_ext", (void *) HandleAndroidDlOpenExt, (void **) &sOriginalAndroidDlOpenExt)) { \
      return (failure_ret_val); \
    } \
  } while (false)

bool memguard::interception::Install() {
    if (!InitializeOriginalFunctions()) {
        return false;
    }

    matrix::PauseLoadSo();
    xhook_block_refresh();

    auto resume = MakeScopeCleaner([]() {
        xhook_unblock_refresh();
        matrix::ResumeLoadSo();
    });

    if (!BeginHook()) {
        return false;
    }

    if (xhook_export_symtable_hook("libc.so", "free",
                                   reinterpret_cast<void*>(HandleFree), nullptr) != 0) {
        LOGE(LOG_TAG, "Fail to do export symtab hook for 'free'.");
        return false;
    }

    if (xhook_export_symtable_hook("libc.so", "realloc",
                                   reinterpret_cast<void*>(HandleReAlloc), nullptr) != 0) {
        LOGE(LOG_TAG, "Fail to do export symtab hook for 'realloc'.");
        return false;
    }

  #define X(pattern, ret_type, sym, args, handler, def_lib_handle) \
    if (!DoHook(HOOK_REQUEST_GROUPID_MEMGUARD, pattern, #sym, (void*) handler, nullptr)) { \
      return false; \
    }

    for (auto & pattern : gOpts.targetSOPatterns) {
        ENUM_C_ALLOC_FUNCTIONS(pattern.c_str())
        // ENUM_CPP_ALLOC_FUNCTIONS(pattern.c_str())
    }

    ENUM_C_DEALLOC_AND_OTHER_FUNCTIONS(".*\\.so$")
    // ENUM_CPP_DEALLOC_FUNCTIONS(".*\\.so$")
  #undef X

    if (!DoHook(HOOK_REQUEST_GROUPID_MEMGUARD, ".*\\.so$", "realloc",
            reinterpret_cast<void*>(HandleReAlloc), nullptr)) {
        return false;
    }

    // Note: Matrix SoLoadMonitor will handle these dlopen stuffs.
    // INTERCEPT_DLOPEN(".*/libnativeloader\\.so$", false);
    // INTERCEPT_DLOPEN(".*/libnativeloader_lazy\\.so$", false);
    // INTERCEPT_DLOPEN(".*/libopenjdk\\.so$", false);
    // INTERCEPT_DLOPEN(".*/libopenjdkjvm\\.so$", false);
    // INTERCEPT_DLOPEN(".*/libart\\.so$", false);
    // INTERCEPT_DLOPEN(".*/libjavacore\\.so$", false);
    // INTERCEPT_DLOPEN(".*/libnativehelper\\.so$", false);

    return EndHook(HOOK_REQUEST_GROUPID_MEMGUARD, gOpts.ignoredSOPatterns);
}