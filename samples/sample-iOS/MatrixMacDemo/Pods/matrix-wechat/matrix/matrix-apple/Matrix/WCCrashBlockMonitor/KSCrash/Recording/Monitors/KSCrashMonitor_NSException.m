//
//  KSCrashMonitor_NSException.m
//
//  Created by Karl Stenerud on 2012-01-28.
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


#import "KSCrashMonitor_NSException.h"
#import "KSStackCursor_Backtrace.h"
#include "KSCrashMonitorContext.h"
#include "KSID.h"
#include "KSThread.h"
#import <Foundation/Foundation.h>

//#define KSLogger_LocalLevel TRACE
#import "KSLogger.h"


// ============================================================================
#pragma mark - Globals -
// ============================================================================

static volatile bool g_isEnabled = 0;

static KSCrash_MonitorContext g_monitorContext;

/** The exception handler that was in place before we installed ours. */
static NSUncaughtExceptionHandler* g_previousUncaughtExceptionHandler;


// ============================================================================
#pragma mark - Callbacks -
// ============================================================================

/** Our custom excepetion handler.
 * Fetch the stack trace from the exception and write a report.
 *
 * @param exception The exception that was raised.
 */
static void handleException(NSException* exception)
{
    KSLOG_DEBUG(@"Trapped exception %@", exception);
    if(g_isEnabled)
    {
        ksmc_suspendEnvironment();
        kscm_notifyFatalExceptionCaptured(false);

        KSLOG_DEBUG(@"Filling out context.");
        NSArray* addresses = [exception callStackReturnAddresses];
        NSUInteger numFrames = addresses.count;
        uintptr_t* callstack = malloc(numFrames * sizeof(*callstack));
        for(NSUInteger i = 0; i < numFrames; i++)
        {
            callstack[i] = (uintptr_t)[addresses[i] unsignedLongLongValue];
        }

        char eventID[37];
        ksid_generate(eventID);
        KSMC_NEW_CONTEXT(machineContext);
        ksmc_getContextForThread(ksthread_self(), machineContext, true);
        KSStackCursor cursor;
        kssc_initWithBacktrace(&cursor, callstack, (int)numFrames, 0);

        KSCrash_MonitorContext* crashContext = &g_monitorContext;
        memset(crashContext, 0, sizeof(*crashContext));
        crashContext->crashType = KSCrashMonitorTypeNSException;
        crashContext->eventID = eventID;
        crashContext->offendingMachineContext = machineContext;
        crashContext->registersAreValid = false;
        crashContext->NSException.name = [[exception name] UTF8String];
        crashContext->exceptionName = crashContext->NSException.name;
        crashContext->crashReason = [[exception reason] UTF8String];
        crashContext->stackCursor = &cursor;

        ksTmpSingal.si_signo = SIGABRT;
        ksTmpSingal.si_pid = (int)ksmc_getThreadFromContext(machineContext);

        KSLOG_DEBUG(@"Calling main crash handler.");
        kscm_handleException(crashContext);
        ksmc_resumeEnvironment();
        kscm_innerHandleSignal(&ksTmpSingal);
        kscm_handleSignal(&ksTmpSingal);

        free(callstack);

        if (g_previousUncaughtExceptionHandler != NULL)
        {
            KSLOG_DEBUG(@"Calling original exception handler.");
            g_previousUncaughtExceptionHandler(exception);
        }
        
    }
}


// ============================================================================
#pragma mark - API -
// ============================================================================

static void setEnabled(bool isEnabled)
{
    if(isEnabled != g_isEnabled)
    {
        g_isEnabled = isEnabled;
        if(isEnabled)
        {
            KSLOG_DEBUG(@"Backing up original handler.");
            g_previousUncaughtExceptionHandler = NSGetUncaughtExceptionHandler();
            
            KSLOG_DEBUG(@"Setting new handler.");
            NSSetUncaughtExceptionHandler(&handleException);
        }
        else
        {
            KSLOG_DEBUG(@"Restoring original handler.");
            NSSetUncaughtExceptionHandler(g_previousUncaughtExceptionHandler);
        }
    }
}

static bool isEnabled()
{
    return g_isEnabled;
}

KSCrashMonitorAPI* kscm_nsexception_getAPI()
{
    static KSCrashMonitorAPI api =
    {
        .setEnabled = setEnabled,
        .isEnabled = isEnabled
    };
    return &api;
}
