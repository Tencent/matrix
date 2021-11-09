//
// Created by tomystang on 2020/10/16.
//

#include <bits/signal_types.h>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <jni/C2Java.h>
#include <util/Log.h>
#include <util/Issue.h>
#include <util/SignalHandler.h>
#include <util/Thread.h>
#include <Options.h>
#include <jni/JNIAux.h>
#include <unistd.h>
#include "Allocation.h"

using namespace memguard;

#define LOG_TAG "MemGuard.SignalHandler"

static struct sigaction sPrevSigAction = {};

struct RoutineParams {
    siginfo_t* info;
    ucontext_t* ucontext;
};

static int ReportRoutine(void* param) {
    auto actualParams = (RoutineParams*) param;
    return issue::Report(actualParams->info->si_addr, actualParams->ucontext) ? 0 : -1;
}

static int NotifyDumpedIssueRoutine(void* param) {
    UNUSED(param);
    pthread_t hThread;
    pthread_create(&hThread, nullptr, [](void*) -> void* {
        c2j::NotifyOnIssueDumpped(gOpts.issueDumpFilePath.c_str());
        return nullptr;
    }, nullptr);
    pthread_join(hThread, nullptr);
    return 0;
}

static void SegvSignalHandler(int sig, siginfo_t* info, void* ucontext) {
    if (info->si_code == SEGV_ACCERR && allocation::IsAllocatedByThisAllocator(info->si_addr)) {
        Thread reportThread(64 * 1024, Thread::FixStackForART, ucontext);
        RoutineParams params = {.info = info, .ucontext = (ucontext_t*) ucontext};
        int result = 0;
        int error = reportThread.startAndWait(ReportRoutine, &params, &result);
        LOGD(LOG_TAG, "ReportRoutine called, result: %d, error: %d", result, error);
        if (error == Thread::RESULT_SUCCESS) {
            error = reportThread.startAndWait(NotifyDumpedIssueRoutine, &params, &result);
            LOGD(LOG_TAG, "NotifyDumpedIssueRoutine called, result: %d, error: %d", result, error);
        }
    }

    // Process any previous handlers.
    if (((unsigned) sPrevSigAction.sa_flags & SA_SIGINFO) != 0) {
        sPrevSigAction.sa_sigaction(sig, info, ucontext);
    } else if (sPrevSigAction.sa_handler == SIG_DFL) {
        // If the previous handler was the default handler, cause a core dump.
        signal(SIGSEGV, SIG_DFL);
        raise(SIGSEGV);
    } else if (sPrevSigAction.sa_handler == SIG_IGN) {
        // If the previous segv handler was SIGIGN, crash if and only if we were responsible
        // for the crash.
        if ((issue::GetLastIssueType() != IssueType::UNKNOWN && info->si_addr != nullptr)
            || (allocation::IsAllocatedByThisAllocator(info->si_addr))) {
            signal(SIGSEGV, SIG_DFL);
            raise(SIGSEGV);
        }
    } else {
        sPrevSigAction.sa_handler(sig);
    }
}

bool memguard::signalhandler::Install() {
    struct sigaction sigAction = {};
    sigAction.sa_sigaction = SegvSignalHandler;
    sigAction.sa_flags = SA_SIGINFO;
    if (::sigaction(SIGSEGV, &sigAction, &sPrevSigAction) == -1) {
        int errcode = errno;
        LOGE(LOG_TAG, "Fail to call sigaction, error: %d (%s)", errcode, strerror(errcode));
        return false;
    }
    LOGD(LOG_TAG, "SignalHandler was installed.");
    return true;
}