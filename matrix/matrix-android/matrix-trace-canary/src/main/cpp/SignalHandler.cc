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
static bool sStackInstalled = false;
static stack_t sOldStack;
static stack_t sNewStack;

static void installAlternateStackLocked() {
    if (sStackInstalled)
        return;

    memset(&sOldStack, 0, sizeof(sOldStack));
    memset(&sNewStack, 0, sizeof(sNewStack));
    static constexpr unsigned kSigStackSize = std::max(16384, SIGSTKSZ);

    if (sigaltstack(nullptr, &sOldStack) == -1 || !sOldStack.ss_sp || sOldStack.ss_size < kSigStackSize) {
        sNewStack.ss_sp = calloc(1, kSigStackSize);
        sNewStack.ss_size = kSigStackSize;
        if (sigaltstack(&sNewStack, nullptr) == -1) {
            free(sNewStack.ss_sp);
            return;
        }
    }

    sStackInstalled = true;
    ALOGV("Alternative stack installed.");
}

bool SignalHandler::installHandlersLocked() {
    if (sHandlerInstalled) {
        return false;
    }

    if (sigaction(TARGET_SIG, nullptr, &sOldHandlers) == -1) {
        return false;
    }

    struct sigaction sa{};
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

static void restoreAlternateStackLocked() {
    if (!sStackInstalled)
        return;

    stack_t current_stack;
    if (sigaltstack(nullptr, &current_stack) == -1)
        return;

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
}

void SignalHandler::signalHandler(int sig, siginfo_t* info, void* uc) {
    ALOGV("Entered signal handler.");

    std::unique_lock<std::mutex> lock(sHandlerStackMutex);

    for (auto it = sHandlerStack->rbegin(); it != sHandlerStack->rend(); ++it) {
        (*it)->handleSignal(sig, info, uc);
    }

    lock.unlock();
}


SignalHandler::SignalHandler() {
    std::lock_guard<std::mutex> lock(sHandlerStackMutex);

    if (!sHandlerStack)
        sHandlerStack = new std::vector<SignalHandler*>;

    installAlternateStackLocked();
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
