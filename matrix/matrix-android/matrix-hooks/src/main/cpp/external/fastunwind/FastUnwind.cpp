//
// Created by tomystang on 2020/12/15.
//

#include <pthread.h>
#include <elf.h>
#include <sys/auxv.h>
#include <csignal>
#include <sys/ucontext.h>
#include <dlfcn.h>
#include <sstream>
#include <iomanip>
#include <android/log.h>
#include "FastUnwind.h"

using namespace fastunwind;

#define LIKELY(cond) __builtin_expect(!!(cond), 1)
#define UNLIKELY(cond) __builtin_expect(!!(cond), 0)
#define TLSVAR __thread __attribute__((tls_model("initial-exec")))

#if 0 // defined(__aarch64__) arm 8.1a feature, disable so far to avoid compatibility issues in ndk r21.
#define HWCAP2_MTE (1 << 18)

struct ScopedDisableMTE {
    size_t prevTCO = {};

    ScopedDisableMTE() {
        if (IsMTESupported()) {
            __asm__ __volatile__(".arch_extension mte; mrs %0, tco; msr tco, #1" : "=r"(prevTCO));
        }
    }

    ~ScopedDisableMTE() {
        if (IsMTESupported()) {
            __asm__ __volatile__(".arch_extension mte; msr tco, %0" : : "r"(prevTCO));
        }
    }

    static inline bool IsMTESupported() {
        return (::getauxval(AT_HWCAP2) & HWCAP2_MTE) != 0;
    }
};
#else
struct ScopedDisableMTE {
    ScopedDisableMTE() = default;
};
#endif

static uintptr_t TLSVAR sThreadStackTop = 0;

static inline __attribute__((unused)) uintptr_t GetThreadStackTop() {
    uintptr_t threadStackTop = sThreadStackTop;
    if (UNLIKELY(threadStackTop == 0)) {
        pthread_attr_t attr = {};
        if (pthread_getattr_np(pthread_self(), &attr) != 0) {
            return 0;
        }
        void* stackBase = nullptr;
        size_t stackSize = 0;
        pthread_attr_getstack(&attr, &stackBase, &stackSize);
        threadStackTop = (uintptr_t) stackBase + stackSize;
        sThreadStackTop = threadStackTop;
    }
    return threadStackTop;
}

static inline __attribute__((unused)) uintptr_t ClearPACBits(uintptr_t ptr) {
#if defined(__aarch64__)
    register uintptr_t x30 __asm("x30") = ptr;
    // This is a NOP on pre-Armv8.3-A architectures.
    asm("xpaclri" : "+r"(x30));
    return x30;
#else
    return ptr;
#endif
}

static int UnwindImpl(const void* begin_fp, void** pcs, size_t max_count) {
    ScopedDisableMTE __attribute__((unused)) x;

    auto begin = reinterpret_cast<uintptr_t>(begin_fp);
    uintptr_t end = GetThreadStackTop();

    stack_t ss = {};
    if (UNLIKELY(::sigaltstack(nullptr, &ss) == 0 && (ss.ss_flags & SS_ONSTACK) != 0)) {
        end = (uintptr_t) ss.ss_sp + ss.ss_size;
    }

    struct FrameRecord {
        uintptr_t nextFrame;
        uintptr_t returnAddr;
    };

    size_t numFrames = 0;
    while (true) {
        auto* frame = (FrameRecord*) begin;
        if (frame->nextFrame < begin + sizeof(FrameRecord)
            || frame->nextFrame >= end
            || frame->nextFrame % sizeof(void*) != 0) {
            break;
        }
        if (numFrames < max_count) {
            pcs[numFrames++] = (void*) ClearPACBits(frame->returnAddr);
        } else {
            break;
        }
        begin = frame->nextFrame;
    }
    return numFrames;
}

int fastunwind::Unwind(void** pcs, size_t max_count) {
    return UnwindImpl(__builtin_frame_address(0), pcs, max_count);
}

int fastunwind::Unwind(void* ucontext, void** pcs, size_t max_count) {
#if defined (__aarch64__)
    const void* begin_fp = *reinterpret_cast<uint64_t**>(reinterpret_cast<uintptr_t>(ucontext) + 432);
    return UnwindImpl(begin_fp, pcs, max_count);
#elif defined (__i386__)
    const void* begin_fp = *reinterpret_cast<uint32_t**>(reinterpret_cast<uintptr_t>(ucontext) + 48);
    return UnwindImpl(begin_fp, pcs, max_count);
#else
    (void) (ucontext);
    (void) (pcs);
    (void) (max_count);
    return 0;
#endif
}