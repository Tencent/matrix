//
// Created by tomystang on 2020/11/27.
//

#include <cinttypes>
#include <ucontext.h>
#include <linux/sched.h>
#include <linux/futex.h>
#include <functional>
#include <fcntl.h>
#include <sys/prctl.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <cerrno>
#include <sched.h>
#include "Log.h"
#include "Memory.h"
#include "Thread.h"
#include "FdSanWrapper.h"

using namespace memguard;

#define LOG_TAG "MemGuard.Thread"
#define STACK_REGION_TAG "MemGuard.ThreadStack"

static bool IterateSelfMaps(void* buffer, size_t buffer_size, const std::function<bool(uintptr_t, uintptr_t)>& callback) {
    if (buffer == nullptr || buffer_size == 0) {
        LOGE(LOG_TAG, "buffer is null or buffer_size is zero.");
        return false;
    }

    int fd = TEMP_FAILURE_RETRY(syscall(__NR_openat, AT_FDCWD, "/proc/self/maps", O_RDONLY | O_CLOEXEC));
    if (fd == -1) {
        int errcode = errno;
        LOGE(LOG_TAG, "fail to open /proc/self/maps, error: %s(%d)", strerror(errcode), errcode);
        return false;
    }
    ON_SCOPE_EXIT(syscall(__NR_close, fd));

    char* charBuffer = (char*) buffer;
    size_t start = 0;
    size_t bytesRead = 0;
    bool readComplete = false;
    while (true) {
        ssize_t currBytesRead = TEMP_FAILURE_RETRY(syscall(__NR_read, fd, charBuffer + bytesRead, buffer_size - bytesRead - 1));
        if (currBytesRead <= 0) {
            if (bytesRead == 0) {
                return currBytesRead == 0;
            }
            // Treat the last piece of data as the last line.
            charBuffer[start + bytesRead] = '\n';
            currBytesRead = 1;
            readComplete = true;
        }
        bytesRead += currBytesRead;

        while (bytesRead > 0) {
            char* newline = (char*) memchr(charBuffer + start, '\n', bytesRead);
            if (newline == nullptr) {
                break;
            }
            *newline = '\0';
            char* line = charBuffer + start;
            start = newline - charBuffer + 1;
            size_t lineLength = newline - line + 1;
            bytesRead -= lineLength;

            char* firstBarPos = (char*) memchr(line, '-', lineLength);
            if (firstBarPos != nullptr) {
                *firstBarPos = '\0';
                uintptr_t startAddr = strtoll(line, nullptr, 16);
                uintptr_t endAddr = strtoll(firstBarPos + 1, nullptr, 16);
                if (callback(startAddr, endAddr)) {
                    return true;
                }
            }
        }

        if (readComplete) {
            return true;
        }

        if (start == 0 && bytesRead == buffer_size - 1) {
            LOGE(LOG_TAG, "buffer is too small to hold a single line.");
            return false;
        }

        // Copy any leftover data to the front  of the buffer.
        if (start > 0) {
            if (bytesRead > 0) {
                memmove(charBuffer, charBuffer + start, bytesRead);
            }
            start = 0;
        }
    }
}

// Tricky but probably the only way to find a place for allocating thread stack that
// is satisfied with the stack base limitation of ART runtime.
// Credit to johnwhe.
static std::pair<void*, bool> SearchStackHintForARTStackFixing(size_t stack_size, void* ucontext) {
    uintptr_t sp = 0;
    #if defined (__arm__)
    sp = ((ucontext_t*) ucontext)->uc_mcontext.arm_sp;
    #elif defined (__aarch64__)
    sp = ((ucontext_t*) ucontext)->uc_mcontext.sp;
    #elif defined (__i386__)
    sp = ((ucontext_t*) ucontext)->uc_mcontext.gregs[REG_ESP];
    #elif defined (__x86_64__)
    sp = ((ucontext_t*) ucontext)->uc_mcontext.gregs[REG_RSP];
    #endif
    if (UNLIKELY(sp == 0)) {
        LOGE(LOG_TAG, "Fail to get sp from ucontext, return empty hint.");
        return std::make_pair(nullptr, false);
    }

    // TODO: constructing std::function by lambda may trigger heap allocation, and may cause
    // problems in signal handler.
    struct {
        size_t stackSize;
        uintptr_t sp;
        uintptr_t lastEnd;
        uintptr_t result;
    } args {stack_size, sp, 0, 0};
    char lineBuffer[1024];
    auto iterCb = [&args](uintptr_t startAddr, uintptr_t endAddr) {
        if (args.sp < args.lastEnd && startAddr - args.lastEnd >= args.stackSize) {
            args.result = args.lastEnd;
            return true;
        }
        args.lastEnd = endAddr;
        return false;
    };
    bool iterOK = IterateSelfMaps(lineBuffer, sizeof(lineBuffer), iterCb);

    if (iterOK && args.result == 0) {
        // Check unused space after last mapped item.
        uintptr_t maxAddr = (((uintptr_t) 1) << (sizeof(void*) * 8 - 1));
        if (maxAddr - args.lastEnd >= stack_size) {
            args.result = args.lastEnd;
        }
    }

    void* finalResult = nullptr;
    bool force;
    if (!iterOK || args.result == 0) {
        finalResult = (void*) AlignUpTo(sp + stack_size, memory::GetPageSize());
        force = false;
    } else {
        finalResult = (void*) args.result;
        force = true;
    }

    LOGI(LOG_TAG, "Found available stack address, sp: %p, found_addr: %p, force: %d", sp, finalResult, force);
    return std::make_pair(finalResult, force);
}

memguard::Thread::Thread(size_t stack_size, const struct Thread::FixStackForART&, void* ucontext)
      : Thread(stack_size, SearchStackHintForARTStackFixing(stack_size, ucontext)) {}

memguard::Thread::Thread(size_t stack_size, const std::pair<void*, bool>& hint) {
    mStackSize = 0;

    void* stackHint = hint.first;
    bool forceHint = hint.second;

    mStack = memory::MapMemArea(stack_size, STACK_REGION_TAG, stackHint, forceHint);
    if (LIKELY(mStack != nullptr)) {
        LOGD(LOG_TAG, "Stack mapped, size: %" PRIu32 ", stack_hint: %p, force_hint: %d, stack_addr: %p",
             stack_size, stackHint, forceHint, mStack);
        memory::MarkAreaReadWrite(mStack, stack_size);
        mStackSize = stack_size;
    } else if (forceHint) {
        LOGE(LOG_TAG, "Fail to allocate %" PRIu32 " bytes stack with hint addr: %p, size: %" PRIu32, stack_size, stackHint);
    }
    if (mStack == nullptr) {
        mStack = memory::MapMemArea(stack_size, STACK_REGION_TAG, stackHint, false);
        if (LIKELY(mStack != nullptr)) {
            LOGD(LOG_TAG, "Stack mapped, size: %" PRIu32 ", stack_hint: %p, force_hint: %d, stack_addr: %p",
                 stack_size, stackHint, forceHint, mStack);
            memory::MarkAreaReadWrite(mStack, stack_size);
            mStackSize = stack_size;
        } else {
            LOGE(LOG_TAG, "Fail to allocate %" PRIu32 " bytes stack again with hint addr: %p, size: %" PRIu32, stack_size, stackHint);
        }
    }
}

struct Context {
    memguard::Thread::RoutineFunction routine_function;
    void* param;
    pid_t tid;
    int return_value;

    explicit Context(memguard::Thread::RoutineFunction routine_function, void* param)
        : routine_function(routine_function), param(param), tid(-1), return_value(0) {}
};

static int ThreadInternalRoutine(void* param) {
    auto origFDSanLevel = AndroidFdSanSetErrorLevel(android_fdsan_error_level::ANDROID_FDSAN_ERROR_LEVEL_DISABLED);
    Context* context = (Context*) param;
    context->return_value = context->routine_function(context->param);
    AndroidFdSanSetErrorLevel(origFDSanLevel);
    return context->return_value;
}

int memguard::Thread::startAndWait(Thread::RoutineFunction routine, void* param, int* return_value) {
    if (mStack == nullptr) {
        return RESULT_INITIALIZE_FAILED;
    }

    int flags = CLONE_UNTRACED | CLONE_VM | CLONE_SIGHAND
          | CLONE_THREAD | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

    Context context(routine, param);

    pid_t child = clone(ThreadInternalRoutine, (void*) ((uint64_t) mStack + mStackSize),
          flags, &context, &context.tid, nullptr, &context.tid);
    if (child == -1) {
        return RESULT_CLONE_FAILED;
    }

    // Wait for thread starting.
    while (context.tid) {
        syscall(__NR_futex, &context.tid, FUTEX_WAIT, -1, nullptr, nullptr, 0);
    }

    // Wait for thread stopping.
    while (context.tid) {
        // clone will unblock us from waiting on context.tid.
        syscall(__NR_futex, &context.tid, FUTEX_WAIT, child, nullptr, nullptr, 0);
    }

    if (return_value != nullptr) {
        *return_value = context.return_value;
    }

    return RESULT_SUCCESS;
}

void memguard::Thread::destroy() {
    if (mStack != nullptr) {
        memory::UnmapMemArea(mStack, mStackSize);
        mStackSize = 0;
    }
}
