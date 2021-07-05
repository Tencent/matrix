// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// SignalHandler.cpp

#include "SignalHandler.h"

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

const int TARGET_SIG = SIGQUIT;
struct sigaction sOldHandlers;
bool sHandlerInstalled = false;

static std::vector<SignalHandler*>* sHandlerStack = nullptr;
static std::mutex sHandlerStackMutex;

bool SignalHandler::installHandlersLocked() {
    if (sHandlerInstalled) {
        return false;
    }

    if (sigaction(TARGET_SIG, nullptr, &sOldHandlers) == -1) {
        return false;
    }

    struct sigaction sa;
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO | SA_RESTART;

    if (sigaction(TARGET_SIG, &sa, nullptr) == -1) {
        ALOGV("Signal handler cannot be installed");
    }

    sHandlerInstalled = true;
    ALOGV("Signal handler installed.");
    return true;
}


static void installDefaultHandler(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    sigaction(sig, &sa, nullptr);
}

static void restoreHandlersLocked() {
    if (!sHandlerInstalled)
        return;

    if (sigaction(TARGET_SIG, &sOldHandlers, nullptr) == -1) {
        installDefaultHandler(TARGET_SIG);
    }

    sHandlerInstalled = false;
    ALOGV("Signal handler restored.");
}

void SignalHandler::signalHandler(int sig, siginfo_t* info, void* uc) {
    ALOGV("Entered signal handler.");

    std::unique_lock<std::mutex> lock(sHandlerStackMutex);

    for (auto it = sHandlerStack->rbegin(); it != sHandlerStack->rend(); ++it) {
        (*it)->handleSignal(sig, info, uc);
    }

    lock.unlock();
    ALOGV("Signal handler mutex released.");

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
        restoreHandlersLocked();
    }
}

}   // namespace MatrixTracer
