// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// SignalHandler.cpp

#include "SignalHandler.h"

#include <string.h>
#include <malloc.h>
#include <syscall.h>
#include <dirent.h>
#include <unistd.h>

#include <mutex>
#include <vector>
#include <algorithm>
#include <cinttypes>

#include "Logging.h"
#include "Support.h"

#define SIGNAL_CATCHER_THREAD_NAME "Signal Catcher"
#define SIGNAL_CATCHER_THREAD_SIGBLK 0x1000

namespace MatrixTracer {

// The list of signals which we consider to be crashes. The default action for
// all these signals must be Core (see man 7 signal) because we rethrow the
// signal after handling it and expect that it'll be fatal.
const int kExceptionSignals = SIGQUIT;
struct sigaction sOldHandlers;
bool sHandlerInstalled = false;

// The global signal handler stack. This is needed because there may exist
// multiple SignalHandler instances in a process. Each will have itself
// registered in this stack.
static std::vector<SignalHandler*>* sHandlerStack = nullptr;
static std::mutex sHandlerStackMutex;

// InstallAlternateStackLocked will store the newly installed stack in new_stack
// and (if it exists) the previously installed stack in old_stack.
static stack_t sOldStack;
static stack_t sNewStack;
static bool sStackInstalled = false;

// Runs before crashing: normal context.
static void restoreAlternateStackLocked() {
    if (!sStackInstalled)
        return;

    stack_t current_stack;
    if (sigaltstack(nullptr, &current_stack) == -1)
        return;

    // Only restore the old_stack if the current alternative stack is the one
    // installed by the call to InstallAlternateStackLocked.
    if (current_stack.ss_sp == sNewStack.ss_sp) {
        if (sOldStack.ss_sp) {
            if (sigaltstack(&sOldStack, nullptr) == -1)
                return;
        } else {
            stack_t disable_stack;
            disable_stack.ss_flags = SS_DISABLE;
            if (sigaltstack(&disable_stack, nullptr) == -1)
                return;
        }
    }

    free(sNewStack.ss_sp);
    sStackInstalled = false;

    ALOGV("Alternative stack restored.");
}

// Runs before crashing: normal context.
bool SignalHandler::installHandlersLocked() {
    if (sHandlerInstalled)
        return false;
    // Fail if unable to store all the old handlers.

    if (sigaction(SIGQUIT, nullptr, &sOldHandlers) == -1)
        return false;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    // Mask all exception signals when we're handling one of them.
    sigaddset(&sa.sa_mask, kExceptionSignals);

    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO | SA_RESTART;

    if (sigaction(SIGQUIT, &sa, nullptr) == -1) {
        // At this point it is impractical to back out changes, and so failure to
        // install a signal is intentionally ignored.
    }


    sHandlerInstalled = true;
    ALOGV("Signal handler installed.");
    return true;
}


static void installDefaultHandler(int sig) {
#if defined(__ANDROID__)
    // Android L+ expose signal and sigaction symbols that override the system
    // ones. There is a bug in these functions where a request to set the handler
    // to SIG_DFL is ignored. In that case, an infinite loop is entered as the
    // signal is repeatedly sent to breakpad's signal handler.
    // To work around this, directly call the system's sigaction.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    sigaction(sig, &sa, nullptr);
#else
    signal(sig, SIG_DFL);
#endif
    ALOGV("Default signal handler installed.");
}

// This function runs in a compromised context: see the top of the file.
// Runs on the crashing thread.
static void restoreHandlersLocked() {
    if (!sHandlerInstalled)
        return;

    if (sigaction(kExceptionSignals, &sOldHandlers, nullptr) == -1) {
        installDefaultHandler(kExceptionSignals);
    }

    sHandlerInstalled = false;

    ALOGV("Signal handler restored.");
}

// This function runs in a compromised context: see the top of the file.
// Runs on the crashing thread.
void SignalHandler::signalHandler(int sig, siginfo_t* info, void* uc) {
    ALOGV("Entered signal handler.");

    // All the exception signals are blocked at this point.
    std::unique_lock<std::mutex> lock(sHandlerStackMutex);

    // Sometimes, Breakpad runs inside a process where some other buggy code
    // saves and restores signal handlers temporarily with 'signal'
    // instead of 'sigaction'. This loses the SA_SIGINFO flag associated
    // with this function. As a consequence, the values of 'info' and 'uc'
    // become totally bogus, generally inducing a crash.
    //
    // The following code tries to detect this case. When it does, it
    // resets the signal handlers with sigaction + SA_SIGINFO and returns.
    // This forces the signal to be thrown again, but this time the kernel
//        // will call the function with the right arguments.
    struct sigaction cur_handler;
    if (sigaction(sig, nullptr, &cur_handler) == 0 &&
        cur_handler.sa_sigaction == signalHandler &&
        (cur_handler.sa_flags & SA_SIGINFO) == 0) {
        // Reset signal handler with the right flags.
        sigemptyset(&cur_handler.sa_mask);
        sigaddset(&cur_handler.sa_mask, sig);

        cur_handler.sa_sigaction = signalHandler;
        cur_handler.sa_flags = SA_ONSTACK | SA_SIGINFO;

        if (sigaction(sig, &cur_handler, nullptr) == -1) {
            // When resetting the handler fails, try to reset the
            // default one to avoid an infinite loop here.
            installDefaultHandler(sig);
        }
        return;
    }

    Result handled = NOT_HANDLED;
    for (auto it = sHandlerStack->rbegin(); it != sHandlerStack->rend(); ++it) {
        handled = (*it)->handleSignal(sig, info, uc);

        if (handled != NOT_HANDLED) break;
    }

    // Upon returning from this signal handler, sig will become unmasked and then
    // it will be retriggered. If one of the ExceptionHandlers handled it
    // successfully, restore the default handler. Otherwise, restore the
    // previously installed handler. Then, when the signal is retriggered, it will
    // be delivered to the appropriate handler.
    if (handled == HANDLED) {
        installDefaultHandler(sig);
    } else if (handled == NOT_HANDLED) {
        restoreHandlersLocked();
    }
    lock.unlock();
    ALOGV("Signal handler mutex released.");

    // info->si_code <= 0 if SI_FROMUSER (SI_FROMKERNEL otherwise).
    if (handled != HANDLED_NO_RETRIGGER && (info->si_code <= 0 || sig == SIGABRT)) {
        // This signal was triggered by somebody sending us the signal with kill().
        // In order to retrigger it, we have to queue a new signal by calling
        // kill() ourselves.  The special case (si_pid == 0 && sig == SIGABRT) is
        // due to the kernel sending a SIGABRT from a user request via SysRQ.
        ALOGV("Resend signal %d, code: %d", sig, info->si_code);

        int64_t ret;
        if (info->si_code == SI_TKILL) {
            ret = syscall(SYS_rt_tgsigqueueinfo, getpid(), getpid(), sig, info);
        } else {
            ret = syscall(SYS_rt_sigqueueinfo, getpid(), sig, info);
        }
        if (ret < 0) {
            // If we failed to kill ourselves (e.g. because a sandbox disallows us
            // to do so), we instead resort to terminating our process. This will
            // result in an incorrect exit code.
            _exit(1);
        }
    } else {
        // This was a synchronous signal triggered by a hard fault (e.g. SIGSEGV).
        // No need to reissue the signal. It will automatically trigger again,
        // when we return from the signal handler.
    }
}

SignalHandler::SignalHandler() {
    std::lock_guard<std::mutex> lock(sHandlerStackMutex);

    if (!sHandlerStack)
        sHandlerStack = new std::vector<SignalHandler*>;

    installHandlersLocked();
    sHandlerStack->push_back(this);
}

SignalHandler::~SignalHandler() {
    std::lock_guard<std::mutex> lock(sHandlerStackMutex);

    auto it = std::find(sHandlerStack->begin(), sHandlerStack->end(), this);
    sHandlerStack->erase(it);
    if (sHandlerStack->empty()) {
        delete sHandlerStack;
        sHandlerStack = nullptr;

        restoreAlternateStackLocked();
        restoreHandlersLocked();
    }
}

}   // namespace MatrixTracer
