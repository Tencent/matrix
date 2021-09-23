//
// Created by tomystang on 2020/11/24.
//

#include <sstream>
#include <cinttypes>
#include <cmath>
#include <fcntl.h>
#include <jni/JNIAux.h>
#include <linux/prctl.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <Options.h>
#include <sys/prctl.h>
#include "Auxiliary.h"
#include "Issue.h"
#include "Log.h"
#include "PagePool.h"
#include "Unwind.h"
#include "Memory.h"
#include "Paths.h"

using namespace memguard;

#define LOG_TAG "MemGuard.Issue"

IssueType TLSVAR issue::gLastIssueType = IssueType::UNKNOWN;

void memguard::issue::TriggerIssue(pagepool::slot_t accessing_slot, IssueType type) {
    if (UNLIKELY(accessing_slot == pagepool::SLOT_NONE)) {
        return;
    }
    gLastIssueType = type;
    void* rightSideGuardPageAddr =
          (void*) ((uintptr_t) pagepool::GetGuardedPageStart(accessing_slot) + pagepool::GetSlotSize());
    // Trigger ACCERR here.
    ((char*) rightSideGuardPageAddr)[0] = '\0';
}

IssueType memguard::issue::GetLastIssueType() {
    return gLastIssueType;
}

static std::vector<std::string> GetReadableStackTrace(const void** pcs, size_t count) {
    std::vector<std::string> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        char stack[1024] = {0};
        result.emplace_back(unwind::GetStackElementDescription(pcs[i], stack, 512));
    }
    return result;
}

static void PrintLineV(int fd, const char* fmt, va_list args) {
    char line[1024] = {};
    int bytesPrint = vsnprintf(line, sizeof(line), fmt, args);
    if (fd >= 0) {
        TEMP_FAILURE_RETRY(syscall(__NR_write, fd, line, bytesPrint));
        TEMP_FAILURE_RETRY(syscall(__NR_write, fd, "\n", 1));
    }
}

static void PrintLine(int fd, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char line[1024] = {};
    int bytesPrint = vsnprintf(line, sizeof(line), fmt, args);
    if (fd >= 0) {
        TEMP_FAILURE_RETRY(syscall(__NR_write, fd, line, bytesPrint));
        TEMP_FAILURE_RETRY(syscall(__NR_write, fd, "\n", 1));
    }
    va_end(args);
}

static void PrintStackTrace(int fd, const std::vector<std::string>& stack_trace, const char* header_fmt, ...) {
    va_list headerArgs;
    va_start(headerArgs, header_fmt);
    ON_SCOPE_EXIT(va_end(headerArgs));

    PrintLineV(fd, header_fmt, headerArgs);
    for (size_t i = 0; i < stack_trace.size(); ++i) {
        PrintLine(fd, "    #%02d %s", i, stack_trace[i].c_str());
    }
    fsync(fd);
}

static pagepool::slot_t GetNearestSlotID(const void* addr) {
    if (addr < pagepool::GetGuardedPageStart(0)) {
        return 0;
    }
    pagepool::slot_t slot = pagepool::GetSlotIdOfAddress(addr);
    if (slot != pagepool::SLOT_NONE) {
        if (!pagepool::IsAddressInGuardPage(addr)) {
            return slot;
        }
        size_t pageSize = memory::GetPageSize();
        if ((((uintptr_t) addr % pageSize) * 2 > pageSize)
            && slot < (pagepool::slot_t) (gOpts.maxDetectableAllocationCount - 1)) {
            ++slot;
        }
    }
    return slot;
}

static char* GetCurrentProcessName(char* buf, size_t buf_size) {
    int fd = TEMP_FAILURE_RETRY(syscall(__NR_openat, AT_FDCWD, "/proc/self/cmdline", O_RDONLY, 0));
    if (fd < 0) {
        snprintf(buf, buf_size, "?");
        return buf;
    }
    ON_SCOPE_EXIT(syscall(__NR_close, fd));

    int bytesRead = TEMP_FAILURE_RETRY(syscall(__NR_read, fd, buf, buf_size - 1));
    buf[bytesRead] = '\0';
    return buf;
}

static char* GetReadableTimeStamp(char* buf, size_t buf_size) {
    timeval tv = {};
    gettimeofday(&tv, nullptr);
    uint32_t ms = lrint((float) tv.tv_usec / 1000.0f);
    tv.tv_sec += ms / 1000;
    ms %= 1000;

    tm tmFields = {};
    localtime_r(&tv.tv_sec, &tmFields);

    size_t writtenLen = strftime(buf, buf_size - 5, "%Y-%m-%d %H:%M:%S", &tmFields);
    if (writtenLen < buf_size) {
        snprintf(buf + writtenLen, buf_size - writtenLen, ".%03d", ms);
    }

    return buf;
}

bool memguard::issue::Report(const void* accessing_addr, void* ucontext) {
    pagepool::slot_t slot = GetNearestSlotID(accessing_addr);
    if (slot == pagepool::SLOT_NONE) {
        return false;
    }
    IssueType lastIssueType = GetLastIssueType();
    void* allocatedStart = pagepool::GetAllocatedAddress(slot);
    if (lastIssueType == IssueType::UNKNOWN) {
        if (pagepool::IsSlotBorrowed(slot)) {
            if (allocatedStart != nullptr) {
                lastIssueType = accessing_addr <= allocatedStart ? IssueType::UNDERFLOW : IssueType::OVERFLOW;
            } else {
                lastIssueType = IssueType::UNKNOWN;
            }
        } else {
            lastIssueType = IssueType::USE_AFTER_FREE;
        }
    }

    int fd = -1;
    if (!gOpts.issueDumpFilePath.empty()) {
        fd = TEMP_FAILURE_RETRY(syscall(__NR_openat, AT_FDCWD, gOpts.issueDumpFilePath.c_str(),
              O_RDWR | O_CREAT, paths::kDefaultDataFilePermission));
        if (fd < 0) {
            int errcode = errno;
            LOGE(LOG_TAG, "Error: %s(%d), fail to open file %s for dumping issue.",
                  strerror(errcode), errcode, gOpts.issueDumpFilePath.c_str());
        }
        syscall(__NR_fchmod, fd, paths::kDefaultDataFilePermission);
    }
    ON_SCOPE_EXIT(if (fd >= 0) syscall(__NR_close, fd));

    size_t allocatedSize = pagepool::GetAllocatedSize(slot);
    pid_t accessingThreadId = gettid();
    issue::ThreadName accessingThreadName = {};
    issue::GetSelfThreadName(accessingThreadName);
    void* accessingStackPCs[pagepool::MAX_RECORDED_STACKFRAME_COUNT] = {};
    size_t accessingStackElemCount =
          unwind::UnwindStack(ucontext, accessingStackPCs, pagepool::MAX_RECORDED_STACKFRAME_COUNT);
    auto accessingStackTrace = GetReadableStackTrace((const void**) accessingStackPCs, accessingStackElemCount);

    pid_t allocateThreadId = 0;
    char* allocateThreadName = nullptr;
    pagepool::GetThreadInfoOnBorrow(slot, &allocateThreadId, &allocateThreadName);
    void** allocateStackPCs = nullptr;
    size_t allocateStackElemCount = 0;
    pagepool::GetStackTraceOnBorrow(slot, &allocateStackPCs, &allocateStackElemCount);
    auto allocateStackTrace = GetReadableStackTrace((const void**) allocateStackPCs, allocateStackElemCount);

    pid_t freeThreadId = 0;
    char* freeThreadName = nullptr;
    void** freeStackPCs = nullptr;
    size_t freeStackElemCount = 0;
    std::vector<std::string> freeStackTrace;

    bool printFreeTrace = lastIssueType == IssueType::DOUBLE_FREE || lastIssueType == IssueType::USE_AFTER_FREE;
    if (printFreeTrace) {
        pagepool::GetThreadInfoOnReturn(slot, &freeThreadId, &freeThreadName);
        pagepool::GetStackTraceOnReturn(slot, &freeStackPCs, &freeStackElemCount);
        freeStackTrace = GetReadableStackTrace((const void**) freeStackPCs, freeStackElemCount);
    }

    char line[256] = {};
    PrintLine(fd, "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***");
    PrintLine(fd, "Process: %s", GetCurrentProcessName(line, sizeof(line)));
    PrintLine(fd, "Report Time: %s", GetReadableTimeStamp(line, sizeof(line)));
    switch (lastIssueType) {
        case IssueType::OVERFLOW: {
            size_t overlappedRelOffset = (size_t) accessing_addr - (size_t) allocatedStart - allocatedSize;
            PrintLine(fd,
                      "Issue: Overflow at %p (%" PRIuPTR " bytes after upper bound) of a %" PRIuPTR "-bytes buffer from thread %s(%d).",
                      accessing_addr, overlappedRelOffset, allocatedSize, accessingThreadName, accessingThreadId);
            break;
        }
        case IssueType::UNDERFLOW: {
            size_t overlappedRelOffset = (size_t) allocatedStart - (size_t) accessing_addr - allocatedSize;
            PrintLine(fd,
                      "Issue: Underflow at %p (%" PRIuPTR " bytes before lower bound) of a %" PRIuPTR "-bytes buffer from thread %s(%d).",
                      accessing_addr, overlappedRelOffset, allocatedSize, accessingThreadName, accessingThreadId);
            break;
        }
        case IssueType::USE_AFTER_FREE: {
            PrintLine(fd,
                      "Issue: Use after free on a %" PRIuPTR "-bytes buffer at %p from thread %s(%d).",
                      allocatedSize, accessing_addr, accessingThreadName, accessingThreadId);
            break;
        }
        case IssueType::DOUBLE_FREE: {
            PrintLine(fd,
                      "Issue: Double free on a %" PRIuPTR "-bytes buffer at %p from thread %s(%d).",
                      allocatedSize, accessing_addr, accessingThreadName, accessingThreadId);
            break;
        }
        case IssueType::UNKNOWN:
        default: {
            PrintLine(fd,
                      "Issue: Unknown failure when access a %" PRIuPTR "-bytes buffer at %p from thread %s(%d).",
                      allocatedSize, accessing_addr, accessingThreadName, accessingThreadId);
            break;
        }
    }
    PrintStackTrace(fd, accessingStackTrace, "stacktrace:");
    if (printFreeTrace) {
        PrintStackTrace(fd, freeStackTrace,
                        "free by thread %s(%d):", freeThreadName, freeThreadId);
    }
    PrintStackTrace(fd, allocateStackTrace,
          "allocate by thread %s(%d):", allocateThreadName, allocateThreadId);

    return true;
}

const char* memguard::issue::GetIssueTypeName(memguard::IssueType type) {
    switch (type) {
        case IssueType::OVERFLOW: return "OVERFLOW";
        case IssueType::UNDERFLOW: return "UNDERFLOW";
        case IssueType::USE_AFTER_FREE: return "USE_AFTER_FREE";
        case IssueType::DOUBLE_FREE: return "DOUBLE_FREE";
        case IssueType::UNKNOWN: default: return "UNKNOWN";
    }
}

void memguard::issue::GetSelfThreadName(ThreadName name_out) {
    if (syscall(__NR_prctl, PR_GET_NAME, name_out, 0, 0, 0) != 0) {
        name_out[0] = '\0';
    } else {
        name_out[sizeof(ThreadName) / sizeof(char) - 1] = '\0';
    }
}