//
//  KSCrashMonitor.h
//
//  Created by Karl Stenerud on 2012-02-12.
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


/** Keeps watch for crashes and informs via callback when on occurs.
 */


#ifndef HDR_KSCrashMonitor_h
#define HDR_KSCrashMonitor_h

#ifdef __cplusplus
extern "C" {
#endif


#include "KSCrashMonitorType.h"
#include "KSThread.h"
    
#include <stdbool.h>
#include <sys/signal.h>

struct KSCrash_MonitorContext;


// ============================================================================
#pragma mark - External API -
// ============================================================================

/** Set which monitors are active.
 *
 * @param monitorTypes Which monitors should be active.
 */
void kscm_setActiveMonitors(KSCrashMonitorType monitorTypes);

/** Get the currently active monitors.
 */
KSCrashMonitorType kscm_getActiveMonitors(void);

/** Set the callback to call when an event is captured.
 *
 * @param onEvent Called whenever an event is captured.
 */
void kscm_setEventCallback(void (*onEvent)(struct KSCrash_MonitorContext* monitorContext));

/** Set the callback to call when user dump
 *
 * @param onUserDump Called when user want to dump
 */
void kscm_setUserDumpHandler(void (*onUserDump)(struct KSCrash_MonitorContext* monitorContext, const char *dumpFilePath));

/** Set the callback to call when crash report is written
 *
 * @param KSCrashSentryHandleSignal Called when crash report is written
 */
void kscm_setHandleSignal(void (*KSCrashSentryHandleSignal)(siginfo_t *info));

void kscm_setInnerHandleSingal(void (*KSCrashSentryHandleSignal)(siginfo_t *info));
    
// ============================================================================
#pragma mark - Internal API -
// ============================================================================

typedef struct
{
    void (*setEnabled)(bool isEnabled);
    bool (*isEnabled)(void);
    void (*addContextualInfoToEvent)(struct KSCrash_MonitorContext* eventContext);
} KSCrashMonitorAPI;

/** Notify that a fatal exception has been captured.
 *  This allows the system to take appropriate steps in preparation.
 *
 * @param isAsyncSafeEnvironment If true, only async-safe functions are allowed from now on.
 */
bool kscm_notifyFatalExceptionCaptured(bool isAsyncSafeEnvironment);

/** Start general exception processing.
 *
 * @param context Contextual information about the exception.
 */
void kscm_handleException(struct KSCrash_MonitorContext* context);

/** Start general exception log of the user dump
 *
 * @param context Contextual information about the exception
 *
 * @param dumpFilePath File path used to save the exception log
 */
void kscm_handleUserDump(struct KSCrash_MonitorContext* context, const char *dumpFilePath);
    
static siginfo_t ksTmpSingal = {0};

/** Pass the signal info raise when the crash happened
 *
 * @param info Signal info raise when the crash happend
 */
void kscm_handleSignal(siginfo_t *info);

void kscm_innerHandleSignal(siginfo_t *info);

#ifdef __cplusplus
}
#endif

#endif // HDR_KSCrashMonitor_h
