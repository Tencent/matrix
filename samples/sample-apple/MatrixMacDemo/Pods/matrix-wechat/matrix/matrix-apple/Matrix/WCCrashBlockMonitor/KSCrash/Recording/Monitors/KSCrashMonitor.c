//
//  KSCrashMonitor.c
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


#include "KSCrashMonitor.h"
#include "KSCrashMonitorContext.h"
#include "KSCrashMonitorType.h"

#include "KSCrashMonitor_Deadlock.h"
#include "KSCrashMonitor_MachException.h"
#include "KSCrashMonitor_CPPException.h"
#include "KSCrashMonitor_NSException.h"
#include "KSCrashMonitor_Signal.h"
#include "KSCrashMonitor_System.h"
#include "KSCrashMonitor_User.h"
#include "KSCrashMonitor_AppState.h"
#include "KSCrashMonitor_Zombie.h"
#include "KSDebug.h"
#include "KSThread.h"
#include "KSSystemCapabilities.h"

#include <memory.h>

//#define KSLogger_LocalLevel TRACE
#include "KSLogger.h"


// ============================================================================
#pragma mark - Globals -
// ============================================================================

typedef struct
{
    KSCrashMonitorType monitorType;
    KSCrashMonitorAPI* (*getAPI)(void);
} Monitor;

static Monitor g_monitors[] =
{
#if KSCRASH_HAS_MACH
    {
        .monitorType = KSCrashMonitorTypeMachException,
        .getAPI = kscm_machexception_getAPI,
    },
#endif
#if KSCRASH_HAS_SIGNAL
    {
        .monitorType = KSCrashMonitorTypeSignal,
        .getAPI = kscm_signal_getAPI,
    },
#endif
#if KSCRASH_HAS_OBJC
    {
        .monitorType = KSCrashMonitorTypeNSException,
        .getAPI = kscm_nsexception_getAPI,
    },
    {
        .monitorType = KSCrashMonitorTypeMainThreadDeadlock,
        .getAPI = kscm_deadlock_getAPI,
    },
    {
        .monitorType = KSCrashMonitorTypeZombie,
        .getAPI = kscm_zombie_getAPI,
    },
#endif
    {
        .monitorType = KSCrashMonitorTypeCPPException,
        .getAPI = kscm_cppexception_getAPI,
    },
    {
        .monitorType = KSCrashMonitorTypeUserReported,
        .getAPI = kscm_user_getAPI,
    },
    {
        .monitorType = KSCrashMonitorTypeSystem,
        .getAPI = kscm_system_getAPI,
    },
    {
        .monitorType = KSCrashMonitorTypeApplicationState,
        .getAPI = kscm_appstate_getAPI,
    },
};
static int g_monitorsCount = sizeof(g_monitors) / sizeof(*g_monitors);

KSCrashMonitorType g_activeMonitors = KSCrashMonitorTypeNone;

static bool g_handlingFatalException = false;
static bool g_crashedDuringExceptionHandling = false;
static bool g_requiresAsyncSafety = false;

static void (*g_onExceptionEvent)(struct KSCrash_MonitorContext* monitorContext);
static void (*g_onHandleSignal)(siginfo_t *info);
static void (*g_onInnerHandleSignal)(siginfo_t *info);
static void (*g_onHandleUserDump)(struct KSCrash_MonitorContext* monitorContext, const char *dumpFilePath);

// ============================================================================
#pragma mark - API -
// ============================================================================

static inline KSCrashMonitorAPI* getAPI(Monitor* monitor)
{
    if(monitor != NULL && monitor->getAPI != NULL)
    {
        return monitor->getAPI();
    }
    return NULL;
}

static inline void setMonitorEnabled(Monitor* monitor, bool isEnabled)
{
    KSCrashMonitorAPI* api = getAPI(monitor);
    if(api != NULL && api->setEnabled != NULL)
    {
        api->setEnabled(isEnabled);
    }
}

static inline bool isMonitorEnabled(Monitor* monitor)
{
    KSCrashMonitorAPI* api = getAPI(monitor);
    if(api != NULL && api->isEnabled != NULL)
    {
        return api->isEnabled();
    }
    return false;
}

static inline void addContextualInfoToEvent(Monitor* monitor, struct KSCrash_MonitorContext* eventContext)
{
    KSCrashMonitorAPI* api = getAPI(monitor);
    if(api != NULL && api->addContextualInfoToEvent != NULL)
    {
        api->addContextualInfoToEvent(eventContext);
    }
}

void kscm_setEventCallback(void (*onEvent)(struct KSCrash_MonitorContext* monitorContext))
{
    g_onExceptionEvent = onEvent;
}

void kscm_setUserDumpHandler(void (*onUserDump)(struct KSCrash_MonitorContext* monitorContext, const char *dumpFilePath))
{
    g_onHandleUserDump = onUserDump;
}

void kscm_setHandleSignal(void (*KSCrashSentryHandleSignal)(siginfo_t *info))
{
    g_onHandleSignal = KSCrashSentryHandleSignal;
}

void kscm_setInnerHandleSingal(void (*KSCrashSentryHandleSignal)(siginfo_t *info))
{
    g_onInnerHandleSignal = KSCrashSentryHandleSignal;
}

void kscm_setActiveMonitors(KSCrashMonitorType monitorTypes)
{
    if(ksdebug_isBeingTraced() && (monitorTypes & KSCrashMonitorTypeDebuggerUnsafe))
    {
        static bool hasWarned = false;
        if(!hasWarned)
        {
            hasWarned = true;
            KSLOGBASIC_WARN("    ************************ Crash Handler Notice ************************");
            KSLOGBASIC_WARN("    *     App is running in a debugger. Masking out unsafe monitors.     *");
            KSLOGBASIC_WARN("    * This means that most crashes WILL NOT BE RECORDED while debugging! *");
            KSLOGBASIC_WARN("    **********************************************************************");
        }
        monitorTypes &= KSCrashMonitorTypeDebuggerSafe;
    }
    if(g_requiresAsyncSafety && (monitorTypes & KSCrashMonitorTypeAsyncUnsafe))
    {
        KSLOG_DEBUG("Async-safe environment detected. Masking out unsafe monitors.");
        monitorTypes &= KSCrashMonitorTypeAsyncSafe;
    }

    KSLOG_DEBUG("Changing active monitors from 0x%x tp 0x%x.", g_activeMonitors, monitorTypes);

    KSCrashMonitorType activeMonitors = KSCrashMonitorTypeNone;
    for(int i = 0; i < g_monitorsCount; i++)
    {
        Monitor* monitor = &g_monitors[i];
        bool isEnabled = monitor->monitorType & monitorTypes;
        setMonitorEnabled(monitor, isEnabled);
        if(isMonitorEnabled(monitor))
        {
            activeMonitors |= monitor->monitorType;
        }
        else
        {
            activeMonitors &= ~monitor->monitorType;
        }
    }

    KSLOG_DEBUG("Active monitors are now 0x%x.", activeMonitors);
    g_activeMonitors = activeMonitors;
}

KSCrashMonitorType kscm_getActiveMonitors()
{
    return g_activeMonitors;
}

// ============================================================================
#pragma mark - Private API -
// ============================================================================

bool kscm_notifyFatalExceptionCaptured(bool isAsyncSafeEnvironment)
{
    g_requiresAsyncSafety |= isAsyncSafeEnvironment; // Don't let it be unset.
    if(g_handlingFatalException)
    {
        g_crashedDuringExceptionHandling = true;
    }
    g_handlingFatalException = true;
    if(g_crashedDuringExceptionHandling)
    {
        KSLOG_INFO("Detected crash in the crash reporter. Uninstalling KSCrash.");
        kscm_setActiveMonitors(KSCrashMonitorTypeNone);
    }
    return g_crashedDuringExceptionHandling;
}

void kscm_handleException(struct KSCrash_MonitorContext* context)
{
    context->requiresAsyncSafety = g_requiresAsyncSafety;
    if(g_crashedDuringExceptionHandling)
    {
        context->crashedDuringCrashHandling = true;
    }
    for(int i = 0; i < g_monitorsCount; i++)
    {
        Monitor* monitor = &g_monitors[i];
        if(isMonitorEnabled(monitor))
        {
            addContextualInfoToEvent(monitor, context);
        }
    }

    g_onExceptionEvent(context);

    if(g_handlingFatalException && !g_crashedDuringExceptionHandling)
    {
        KSLOG_DEBUG("Exception is fatal. Restoring original handlers.");
        kscm_setActiveMonitors(KSCrashMonitorTypeNone);
    }
}

void kscm_handleUserDump(struct KSCrash_MonitorContext* context, const char *dumpFilePath)
{
    context->requiresAsyncSafety = g_requiresAsyncSafety;
    if(g_crashedDuringExceptionHandling)
    {
        context->crashedDuringCrashHandling = true;
    }
    for(int i = 0; i < g_monitorsCount; i++)
    {
        Monitor* monitor = &g_monitors[i];
        if(isMonitorEnabled(monitor))
        {
            addContextualInfoToEvent(monitor, context);
        }
    }
    
    g_onHandleUserDump(context, dumpFilePath);
    
    if(g_handlingFatalException && !g_crashedDuringExceptionHandling)
    {
        KSLOG_DEBUG("Exception is fatal. Restoring original handlers.");
        kscm_setActiveMonitors(KSCrashMonitorTypeNone);
    }
}

void kscm_handleSignal(siginfo_t *info)
{
    if (g_onHandleSignal != NULL) {
        g_onHandleSignal(info);
    }
}

void kscm_innerHandleSignal(siginfo_t *info)
{
    if (g_onInnerHandleSignal != NULL) {
        g_onInnerHandleSignal(info);
    }
}
