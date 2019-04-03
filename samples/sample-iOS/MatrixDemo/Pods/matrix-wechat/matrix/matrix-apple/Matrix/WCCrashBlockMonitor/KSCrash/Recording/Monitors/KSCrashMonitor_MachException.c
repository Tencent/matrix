//
//  KSCrashMonitor_MachException.c
//
//  Created by Karl Stenerud on 2012-02-04.
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


#include "KSCrashMonitor_MachException.h"
#include "KSCrashMonitorContext.h"
#include "KSCPU.h"
#include "KSID.h"
#include "KSThread.h"
#include "KSSystemCapabilities.h"
#include "KSStackCursor_MachineContext.h"

//#define KSLogger_LocalLevel TRACE
#include "KSLogger.h"

#if KSCRASH_HAS_MACH

#include <mach/mach.h>
#include <pthread.h>
#include <signal.h>


// ============================================================================
#pragma mark - Constants -
// ============================================================================

#define kThreadPrimary "KSCrash Exception Handler (Primary)"
#define kThreadSecondary "KSCrash Exception Handler (Secondary)"


// ============================================================================
#pragma mark - Types -
// ============================================================================

/** A mach exception message (according to ux_exception.c, xnu-1699.22.81).
 */
typedef struct
{
    /** Mach header. */
    mach_msg_header_t          header;

    // Start of the kernel processed data.

    /** Basic message body data. */
    mach_msg_body_t            body;

    /** The thread that raised the exception. */
    mach_msg_port_descriptor_t thread;

    /** The task that raised the exception. */
    mach_msg_port_descriptor_t task;

    // End of the kernel processed data.

    /** Network Data Representation. */
    NDR_record_t               NDR;

    /** The exception that was raised. */
    exception_type_t           exception;

    /** The number of codes. */
    mach_msg_type_number_t     codeCount;

    /** Exception code and subcode. */
    // ux_exception.c defines this as mach_exception_data_t for some reason.
    // But it's not actually a pointer; it's an embedded array.
    // On 32-bit systems, only the lower 32 bits of the code and subcode
    // are valid.
    mach_exception_data_type_t code[0];

    /** Padding to avoid RCV_TOO_LARGE. */
    char                       padding[512];
} MachExceptionMessage;

/** A mach reply message (according to ux_exception.c, xnu-1699.22.81).
 */
typedef struct
{
    /** Mach header. */
    mach_msg_header_t header;

    /** Network Data Representation. */
    NDR_record_t      NDR;

    /** Return code. */
    kern_return_t     returnCode;
} MachReplyMessage;


// ============================================================================
#pragma mark - Globals -
// ============================================================================

static volatile bool g_isEnabled = false;

static KSCrash_MonitorContext g_monitorContext;
static KSStackCursor g_stackCursor;

static bool g_isHandlingCrash = false;

/** Holds exception port info regarding the previously installed exception
 * handlers.
 */
static struct
{
    exception_mask_t        masks[EXC_TYPES_COUNT];
    exception_handler_t     ports[EXC_TYPES_COUNT];
    exception_behavior_t    behaviors[EXC_TYPES_COUNT];
    thread_state_flavor_t   flavors[EXC_TYPES_COUNT];
    mach_msg_type_number_t  count;
} g_previousExceptionPorts;

/** Our exception port. */
static mach_port_t g_exceptionPort = MACH_PORT_NULL;

/** Primary exception handler thread. */
static pthread_t g_primaryPThread;
static thread_t g_primaryMachThread;

/** Secondary exception handler thread in case crash handler crashes. */
static pthread_t g_secondaryPThread;
static thread_t g_secondaryMachThread;

static char g_primaryEventID[37];
static char g_secondaryEventID[37];

// ============================================================================
#pragma mark - Utility -
// ============================================================================

/** Restore the original mach exception ports.
 */
static void restoreExceptionPorts(void)
{
    KSLOG_DEBUG("Restoring original exception ports.");
    if(g_previousExceptionPorts.count == 0)
    {
        KSLOG_DEBUG("Original exception ports were already restored.");
        return;
    }

    const task_t thisTask = mach_task_self();
    kern_return_t kr;

    // Reinstall old exception ports.
    for(mach_msg_type_number_t i = 0; i < g_previousExceptionPorts.count; i++)
    {
        KSLOG_TRACE("Restoring port index %d", i);
        kr = task_set_exception_ports(thisTask,
                                      g_previousExceptionPorts.masks[i],
                                      g_previousExceptionPorts.ports[i],
                                      g_previousExceptionPorts.behaviors[i],
                                      g_previousExceptionPorts.flavors[i]);
        if(kr != KERN_SUCCESS)
        {
            KSLOG_ERROR("task_set_exception_ports: %s",
                        mach_error_string(kr));
        }
    }
    KSLOG_DEBUG("Exception ports restored.");
    g_previousExceptionPorts.count = 0;
}

#define EXC_UNIX_BAD_SYSCALL 0x10000 /* SIGSYS */
#define EXC_UNIX_BAD_PIPE    0x10001 /* SIGPIPE */
#define EXC_UNIX_ABORT       0x10002 /* SIGABRT */

static int signalForMachException(exception_type_t exception, mach_exception_code_t code)
{
    switch(exception)
    {
        case EXC_ARITHMETIC:
            return SIGFPE;
        case EXC_BAD_ACCESS:
            return code == KERN_INVALID_ADDRESS ? SIGSEGV : SIGBUS;
        case EXC_BAD_INSTRUCTION:
            return SIGILL;
        case EXC_BREAKPOINT:
            return SIGTRAP;
        case EXC_EMULATION:
            return SIGEMT;
        case EXC_SOFTWARE:
        {
            switch (code)
            {
                case EXC_UNIX_BAD_SYSCALL:
                    return SIGSYS;
                case EXC_UNIX_BAD_PIPE:
                    return SIGPIPE;
                case EXC_UNIX_ABORT:
                    return SIGABRT;
                case EXC_SOFT_SIGNAL:
                    return SIGKILL;
            }
            break;
        }
    }
    return 0;
}

static exception_type_t machExceptionForSignal(int sigNum)
{
    switch(sigNum)
    {
        case SIGFPE:
            return EXC_ARITHMETIC;
        case SIGSEGV:
            return EXC_BAD_ACCESS;
        case SIGBUS:
            return EXC_BAD_ACCESS;
        case SIGILL:
            return EXC_BAD_INSTRUCTION;
        case SIGTRAP:
            return EXC_BREAKPOINT;
        case SIGEMT:
            return EXC_EMULATION;
        case SIGSYS:
            return EXC_UNIX_BAD_SYSCALL;
        case SIGPIPE:
            return EXC_UNIX_BAD_PIPE;
        case SIGABRT:
            // The Apple reporter uses EXC_CRASH instead of EXC_UNIX_ABORT
            return EXC_CRASH;
        case SIGKILL:
            return EXC_SOFT_SIGNAL;
    }
    return 0;
}

// ============================================================================
#pragma mark - Handler -
// ============================================================================

/** Our exception handler thread routine.
 * Wait for an exception message, uninstall our exception port, record the
 * exception information, and write a report.
 */
static void* handleExceptions(void* const userData)
{
    MachExceptionMessage exceptionMessage = {{0}};
    MachReplyMessage replyMessage = {{0}};
    char* eventID = g_primaryEventID;

    const char* threadName = (const char*) userData;
    pthread_setname_np(threadName);
    if(strcmp(threadName,kThreadSecondary))
    {
        KSLOG_DEBUG("This is the secondary thread. Suspending.");
        thread_suspend((thread_t)ksthread_self());
        eventID = g_secondaryEventID;
    }

    for(;;)
    {
        KSLOG_DEBUG("Waiting for mach exception");

        // Wait for a message.
        kern_return_t kr = mach_msg(&exceptionMessage.header,
                                    MACH_RCV_MSG,
                                    0,
                                    sizeof(exceptionMessage),
                                    g_exceptionPort,
                                    MACH_MSG_TIMEOUT_NONE,
                                    MACH_PORT_NULL);
        if(kr == KERN_SUCCESS)
        {
            break;
        }

        // Loop and try again on failure.
        KSLOG_ERROR("mach_msg: %s", mach_error_string(kr));
    }

    KSLOG_DEBUG("Trapped mach exception code 0x%x, subcode 0x%x",
                exceptionMessage.code[0], exceptionMessage.code[1]);
    if(g_isEnabled)
    {
        ksmc_suspendEnvironment();
        g_isHandlingCrash = true;
        kscm_notifyFatalExceptionCaptured(true);

        KSLOG_DEBUG("Exception handler is installed. Continuing exception handling.");


        // Switch to the secondary thread if necessary, or uninstall the handler
        // to avoid a death loop.
        if(ksthread_self() == g_primaryMachThread)
        {
            KSLOG_DEBUG("This is the primary exception thread. Activating secondary thread.");
// TODO: This was put here to avoid a freeze. Does secondary thread ever fire?
            restoreExceptionPorts();
            if(thread_resume(g_secondaryMachThread) != KERN_SUCCESS)
            {
                KSLOG_DEBUG("Could not activate secondary thread. Restoring original exception ports.");
            }
        }
        else
        {
            KSLOG_DEBUG("This is the secondary exception thread. Restoring original exception ports.");
//            restoreExceptionPorts();
        }

        // Fill out crash information
        KSLOG_DEBUG("Fetching machine state.");
        KSMC_NEW_CONTEXT(machineContext);
        KSCrash_MonitorContext* crashContext = &g_monitorContext;
        crashContext->offendingMachineContext = machineContext;
        kssc_initCursor(&g_stackCursor, NULL, NULL);
        if(ksmc_getContextForThread(exceptionMessage.thread.name, machineContext, true))
        {
            kssc_initWithMachineContext(&g_stackCursor, 10000, machineContext);
            KSLOG_TRACE("Fault address 0x%x, instruction address 0x%x", kscpu_faultAddress(machineContext), kscpu_instructionAddress(machineContext));
            if(exceptionMessage.exception == EXC_BAD_ACCESS)
            {
                crashContext->faultAddress = kscpu_faultAddress(machineContext);
            }
            else
            {
                crashContext->faultAddress = kscpu_instructionAddress(machineContext);
            }
        }

        KSLOG_DEBUG("Filling out context.");
        crashContext->crashType = KSCrashMonitorTypeMachException;
        crashContext->eventID = eventID;
        crashContext->registersAreValid = true;
        crashContext->mach.type = exceptionMessage.exception;
        crashContext->mach.code = exceptionMessage.code[0];
        crashContext->mach.subcode = exceptionMessage.code[1];
        if(crashContext->mach.code == KERN_PROTECTION_FAILURE && crashContext->isStackOverflow)
        {
            // A stack overflow should return KERN_INVALID_ADDRESS, but
            // when a stack blasts through the guard pages at the top of the stack,
            // it generates KERN_PROTECTION_FAILURE. Correct for this.
            crashContext->mach.code = KERN_INVALID_ADDRESS;
        }
        crashContext->signal.signum = signalForMachException(crashContext->mach.type, crashContext->mach.code);
        crashContext->stackCursor = &g_stackCursor;

        ksTmpSingal.si_pid = (int)ksmc_getThreadFromContext(machineContext);
        ksTmpSingal.si_signo = crashContext->signal.signum;

        kscm_handleException(crashContext);

        KSLOG_DEBUG("Crash handling complete. Restoring original handlers.");
        g_isHandlingCrash = false;
        ksmc_resumeEnvironment();
        kscm_innerHandleSignal(&ksTmpSingal);
        kscm_handleSignal(&ksTmpSingal);        
    }

    KSLOG_DEBUG("Replying to mach exception message.");
    // Send a reply saying "I didn't handle this exception".
    replyMessage.header = exceptionMessage.header;
    replyMessage.NDR = exceptionMessage.NDR;
    replyMessage.returnCode = KERN_FAILURE;

    mach_msg(&replyMessage.header,
             MACH_SEND_MSG,
             sizeof(replyMessage),
             0,
             MACH_PORT_NULL,
             MACH_MSG_TIMEOUT_NONE,
             MACH_PORT_NULL);

    return NULL;
}


// ============================================================================
#pragma mark - API -
// ============================================================================

static void uninstallExceptionHandler()
{
    KSLOG_DEBUG("Uninstalling mach exception handler.");
    
    // NOTE: Do not deallocate the exception port. If a secondary crash occurs
    // it will hang the process.
    
    restoreExceptionPorts();
    
    thread_t thread_self = (thread_t)ksthread_self();
    
    if(g_primaryPThread != 0 && g_primaryMachThread != thread_self)
    {
        KSLOG_DEBUG("Canceling primary exception thread.");
        if(g_isHandlingCrash)
        {
            thread_terminate(g_primaryMachThread);
        }
        else
        {
            pthread_cancel(g_primaryPThread);
        }
        g_primaryMachThread = 0;
        g_primaryPThread = 0;
    }
    if(g_secondaryPThread != 0 && g_secondaryMachThread != thread_self)
    {
        KSLOG_DEBUG("Canceling secondary exception thread.");
        if(g_isHandlingCrash)
        {
            thread_terminate(g_secondaryMachThread);
        }
        else
        {
            pthread_cancel(g_secondaryPThread);
        }
        g_secondaryMachThread = 0;
        g_secondaryPThread = 0;
    }
    
    KSLOG_DEBUG("Mach exception handlers uninstalled.");
}

static bool installExceptionHandler()
{
    KSLOG_DEBUG("Installing mach exception handler.");

    bool attributes_created = false;
    pthread_attr_t attr;

    kern_return_t kr;
    int error;

    const task_t thisTask = mach_task_self();
    exception_mask_t mask = EXC_MASK_BAD_ACCESS |
    EXC_MASK_BAD_INSTRUCTION |
    EXC_MASK_ARITHMETIC |
    EXC_MASK_SOFTWARE |
    EXC_MASK_BREAKPOINT;

    KSLOG_DEBUG("Backing up original exception ports.");
    kr = task_get_exception_ports(thisTask,
                                  mask,
                                  g_previousExceptionPorts.masks,
                                  &g_previousExceptionPorts.count,
                                  g_previousExceptionPorts.ports,
                                  g_previousExceptionPorts.behaviors,
                                  g_previousExceptionPorts.flavors);
    if(kr != KERN_SUCCESS)
    {
        KSLOG_ERROR("task_get_exception_ports: %s", mach_error_string(kr));
        goto failed;
    }

    if(g_exceptionPort == MACH_PORT_NULL)
    {
        KSLOG_DEBUG("Allocating new port with receive rights.");
        kr = mach_port_allocate(thisTask,
                                MACH_PORT_RIGHT_RECEIVE,
                                &g_exceptionPort);
        if(kr != KERN_SUCCESS)
        {
            KSLOG_ERROR("mach_port_allocate: %s", mach_error_string(kr));
            goto failed;
        }

        KSLOG_DEBUG("Adding send rights to port.");
        kr = mach_port_insert_right(thisTask,
                                    g_exceptionPort,
                                    g_exceptionPort,
                                    MACH_MSG_TYPE_MAKE_SEND);
        if(kr != KERN_SUCCESS)
        {
            KSLOG_ERROR("mach_port_insert_right: %s", mach_error_string(kr));
            goto failed;
        }
    }

    KSLOG_DEBUG("Installing port as exception handler.");
    kr = task_set_exception_ports(thisTask,
                                  mask,
                                  g_exceptionPort,
                                  EXCEPTION_DEFAULT,
                                  THREAD_STATE_NONE);
    if(kr != KERN_SUCCESS)
    {
        KSLOG_ERROR("task_set_exception_ports: %s", mach_error_string(kr));
        goto failed;
    }

    KSLOG_DEBUG("Creating secondary exception thread (suspended).");
    pthread_attr_init(&attr);
    attributes_created = true;
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    error = pthread_create(&g_secondaryPThread,
                           &attr,
                           &handleExceptions,
                           kThreadSecondary);
    if(error != 0)
    {
        KSLOG_ERROR("pthread_create_suspended_np: %s", strerror(error));
        goto failed;
    }
    g_secondaryMachThread = pthread_mach_thread_np(g_secondaryPThread);
    ksmc_addReservedThread(g_secondaryMachThread);

    KSLOG_DEBUG("Creating primary exception thread.");
    error = pthread_create(&g_primaryPThread,
                           &attr,
                           &handleExceptions,
                           kThreadPrimary);
    if(error != 0)
    {
        KSLOG_ERROR("pthread_create: %s", strerror(error));
        goto failed;
    }
    pthread_attr_destroy(&attr);
    g_primaryMachThread = pthread_mach_thread_np(g_primaryPThread);
    ksmc_addReservedThread(g_primaryMachThread);

    KSLOG_DEBUG("Mach exception handler installed.");
    return true;


failed:
    KSLOG_DEBUG("Failed to install mach exception handler.");
    if(attributes_created)
    {
        pthread_attr_destroy(&attr);
    }
    uninstallExceptionHandler();
    return false;
}

static void setEnabled(bool isEnabled)
{
    if(isEnabled != g_isEnabled)
    {
        g_isEnabled = isEnabled;
        if(isEnabled)
        {
            ksid_generate(g_primaryEventID);
            ksid_generate(g_secondaryEventID);
            if(!installExceptionHandler())
            {
                return;
            }
        }
        else
        {
            uninstallExceptionHandler();
        }
    }
}

static bool isEnabled()
{
    return g_isEnabled;
}

static void addContextualInfoToEvent(struct KSCrash_MonitorContext* eventContext)
{
    if(eventContext->crashType == KSCrashMonitorTypeSignal)
    {
        eventContext->mach.type = machExceptionForSignal(eventContext->signal.signum);
    }
    else if(eventContext->crashType != KSCrashMonitorTypeMachException)
    {
        eventContext->mach.type = EXC_CRASH;
    }
}

#endif

KSCrashMonitorAPI* kscm_machexception_getAPI()
{
    static KSCrashMonitorAPI api =
    {
#if KSCRASH_HAS_MACH
        .setEnabled = setEnabled,
        .isEnabled = isEnabled,
        .addContextualInfoToEvent = addContextualInfoToEvent
#endif
    };
    return &api;
}
