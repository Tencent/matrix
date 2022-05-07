//
//  KSCrashMonitor_User.c
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "KSCrashMonitor_User.h"
#include "KSCrashMonitorContext.h"
#include "KSID.h"
#include "KSThread.h"
#include "KSStackCursor_SelfThread.h"
#include "KSSnapshot.h"

//#define KSLogger_LocalLevel TRACE
#include "KSLogger.h"

#include <memory.h>
#include <stdlib.h>

/** Context to fill with crash information. */

static volatile bool g_isEnabled = false;

void kscm_reportUserException(const char *name,
                              const char *reason,
                              const char *language,
                              const char *lineOfCode,
                              const char *stackTrace,
                              bool logAllThreads,
                              bool enableSnapshot,
                              bool terminateProgram,
                              bool writeCpuUsage,
                              const char *dumpFilePath,
                              int dumpType) {
    if (!g_isEnabled) {
        KSLOG_WARN("User-reported exception monitor is not installed. Exception has not been recorded.");
    } else {
        char eventID[37];
        // 在ksmc_suspendEnvironment()之后调用sprintf()系列函数，可能导致死锁，因此把这一步提前
        ksid_generate(eventID);

        KSMC_NEW_CONTEXT(machineContext);
        KSMC_NEW_CONTEXT(machineContext_cpuUsage);
        ksmc_getContextForThread(ksthread_self(), machineContext, true);
        if (writeCpuUsage) {
            ksmc_getCpuUsage(machineContext_cpuUsage);
        }

        KSSnapshot snapshot;
        KSSnapshotStatus snapshotStatus = KSSnapshotTurnedOff;

        if (logAllThreads) {
            if (enableSnapshot) {
                snapshotStatus = kssnapshot_initWithMachineContext(&snapshot, machineContext);
            }

            ksmc_suspendEnvironment();

            if (snapshotStatus == KSSnapshotSucceeded) {
                snapshotStatus = kssnapshot_takeSnapshot(&snapshot);
            }
            if (snapshotStatus == KSSnapshotSucceeded) {
                // 快照成功，提前恢复其他线程
                ksmc_resumeEnvironment();
            }
        }

        if (terminateProgram) {
            kscm_notifyFatalExceptionCaptured(false);
        }

        if (writeCpuUsage) {
            ksmc_setCpuUsage(machineContext, machineContext_cpuUsage);
        }

        KSStackCursor stackCursor;
        kssc_initSelfThread(&stackCursor, 0);

        KSLOG_DEBUG("Filling out context.");
        KSCrash_MonitorContext context;
        memset(&context, 0, sizeof(context));
        context.crashType = KSCrashMonitorTypeUserReported;
        context.eventID = eventID;
        context.offendingMachineContext = machineContext;
        context.registersAreValid = false;
        context.crashReason = reason;
        context.userException.name = name;
        context.userException.language = language;
        context.userException.lineOfCode = lineOfCode;
        context.userException.customStackTrace = stackTrace;
        context.userException.userDumpType = dumpType;
        context.userException.writeCpuUsage = writeCpuUsage;
        context.userException.suspendAllThreads = logAllThreads;
        context.userException.snapshotStatus = snapshotStatus;
        context.userException.snapshot = &snapshot;
        context.stackCursor = &stackCursor;

        if (dumpFilePath == NULL) {
            kscm_handleException(&context);
        } else {
            kscm_handleUserDump(&context, dumpFilePath);
        }

        if (logAllThreads) {
            if (snapshotStatus != KSSnapshotSucceeded) {
                ksmc_resumeEnvironment();
            }

            if (enableSnapshot) {
                kssnapshot_deinit(&snapshot);
            }
        }
        if (terminateProgram) {
            abort();
        }
    }
}

static void setEnabled(bool isEnabled) {
    g_isEnabled = isEnabled;
}

static bool isEnabled() {
    return g_isEnabled;
}

KSCrashMonitorAPI *kscm_user_getAPI() {
    static KSCrashMonitorAPI api = { .setEnabled = setEnabled, .isEnabled = isEnabled };
    return &api;
}
