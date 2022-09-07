//
// Created by tomystang on 2020/11/26.
//

#include <asm/unistd.h>
#include <cerrno>
#include <cinttypes>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <util/Memory.h>
#include <util/Log.h>
#include <Options.h>

using namespace memguard;

#define LOG_TAG "MemGuard.Memory"

#define PR_SET_VMA 0x53564d41
#define PR_SET_VMA_ANON_NAME 0

void* memguard::memory::MapMemArea(size_t size, const char* region_name, void* hint, bool force_hint) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (force_hint) {
        flags |= MAP_FIXED;
    }
    #ifdef __NR_mmap2
    void* res = (void*) syscall(__NR_mmap2, hint, size, PROT_NONE, flags, -1, 0);
    #else
    void* res = (void*) syscall(__NR_mmap, hint, size, gOpts.ignoreOverlappedReading ? PROT_READ : PROT_NONE, flags, -1, 0);
    #endif
    if (res == (void*) -1) {
        return nullptr;
    }
#ifdef __ANDROID__
    if (syscall(__NR_prctl, PR_SET_VMA, PR_SET_VMA_ANON_NAME,
          (unsigned long) res, (unsigned long) size, (unsigned long) region_name) != 0) {
        int errcode = errno;
        LOGW(LOG_TAG, "Fail to name anonymous mmaped region, error: %s(%d)", strerror(errcode), errcode);
    }
#endif
    return res;
}

void memguard::memory::UnmapMemArea(void* addr, size_t size) {
    int res = syscall(__NR_munmap, addr, size);
    if (res != 0) {
        int errcode = errno;
        LOGW(LOG_TAG, "Fail to unmap region, error: %s(%d)", strerror(errcode), errcode);
    }
}

void memguard::memory::MarkAreaReadWrite(void* start, size_t size) {
    if (syscall(__NR_mprotect, start, size, PROT_READ | PROT_WRITE) != 0) {
        int errcode = errno;
        LOGF(LOG_TAG, "Fail to change privilege of mark region (%p ~ +%" PRIu32 ") to 'rw', error: %s(%d)",
              start, size, strerror(errcode), errcode);
    }
}

void memguard::memory::MarkAreaInaccessible(void* start, size_t size) {
    if (syscall(__NR_mprotect, start, size, gOpts.ignoreOverlappedReading ? PROT_READ : PROT_NONE) != 0) {
        int errcode = errno;
        LOGF(LOG_TAG, "Fail to change privilege of mark region (%p ~ +%" PRIu32 ") to 'none', error: %s(%d)",
             start, size, strerror(errcode), errcode);
    }
}

size_t memguard::memory::GetPageSize() {
    return sysconf(_SC_PAGESIZE);
}