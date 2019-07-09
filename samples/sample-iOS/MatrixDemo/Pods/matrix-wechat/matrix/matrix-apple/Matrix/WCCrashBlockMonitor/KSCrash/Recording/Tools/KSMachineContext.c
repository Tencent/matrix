//
//  KSMachineContext.c
//
//  Created by Karl Stenerud on 2016-12-02.
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

#include "KSMachineContext_Apple.h"
#include "KSMachineContext.h"
#include "KSSystemCapabilities.h"
#include "KSCPU.h"
#include "KSCPU_Apple.h"
#include "KSStackCursor_MachineContext.h"

#include <mach/mach.h>

//#define KSLogger_LocalLevel TRACE
#include "KSLogger.h"

#ifdef __arm64__
    #define UC_MCONTEXT uc_mcontext64
    typedef ucontext64_t SignalUserContext;
#else
    #define UC_MCONTEXT uc_mcontext
    typedef ucontext_t SignalUserContext;
#endif

static KSThread g_reservedThreads[10];
static int g_reservedThreadsMaxIndex = sizeof(g_reservedThreads) / sizeof(g_reservedThreads[0]) - 1;
static int g_reservedThreadsCount = 0;



static inline bool isStackOverflow(const KSMachineContext* const context)
{
    KSStackCursor stackCursor;
    kssc_initWithMachineContext(&stackCursor, KSSC_STACK_OVERFLOW_THRESHOLD, context);
    while(stackCursor.advanceCursor(&stackCursor))
    {
    }
    return stackCursor.state.hasGivenUp;
}

static inline bool getThreadList(KSMachineContext* context)
{
    const task_t thisTask = mach_task_self();
    KSLOG_DEBUG("Getting thread list");
    kern_return_t kr;
    thread_act_array_t threads;
    mach_msg_type_number_t actualThreadCount;

    if((kr = task_threads(thisTask, &threads, &actualThreadCount)) != KERN_SUCCESS)
    {
        KSLOG_ERROR("task_threads: %s", mach_error_string(kr));
        return false;
    }
    KSLOG_TRACE("Got %d threads", context->threadCount);
    int threadCount = (int)actualThreadCount;
    int maxThreadCount = sizeof(context->allThreads) / sizeof(context->allThreads[0]);
    if(threadCount > maxThreadCount)
    {
        KSLOG_ERROR("Thread count %d is higher than maximum of %d", threadCount, maxThreadCount);
        threadCount = maxThreadCount;
    }
    for(int i = 0; i < threadCount; i++)
    {
        context->allThreads[i] = threads[i];
    }
    context->threadCount = threadCount;

    for(mach_msg_type_number_t i = 0; i < actualThreadCount; i++)
    {
        mach_port_deallocate(thisTask, context->allThreads[i]);
    }
    vm_deallocate(thisTask, (vm_address_t)threads, sizeof(thread_t) * actualThreadCount);

    return true;
}

int ksmc_contextSize()
{
    return sizeof(KSMachineContext);
}

KSThread ksmc_getThreadFromContext(const KSMachineContext* const context)
{
    return context->thisThread;
}

bool ksmc_getContextForThread(KSThread thread, KSMachineContext* destinationContext, bool isCrashedContext)
{
    KSLOG_DEBUG("Fill thread 0x%x context into %p. is crashed = %d", thread, destinationContext, isCrashedContext);
    memset(destinationContext, 0, sizeof(*destinationContext));
    destinationContext->thisThread = (thread_t)thread;
    destinationContext->isCurrentThread = thread == ksthread_self();
    destinationContext->isCrashedContext = isCrashedContext;
    destinationContext->isSignalContext = false;
    if(ksmc_canHaveCPUState(destinationContext))
    {
        kscpu_getState(destinationContext);
    }
    if(ksmc_isCrashedContext(destinationContext))
    {
        destinationContext->isStackOverflow = isStackOverflow(destinationContext);
        getThreadList(destinationContext);
    }
    KSLOG_TRACE("Context retrieved.");
    return true;
}

bool ksmc_getContextForSignal(void* signalUserContext, KSMachineContext* destinationContext)
{
    KSLOG_DEBUG("Get context from signal user context and put into %p.", destinationContext);
    _STRUCT_MCONTEXT* sourceContext = ((SignalUserContext*)signalUserContext)->UC_MCONTEXT;
    memcpy(&destinationContext->machineContext, sourceContext, sizeof(destinationContext->machineContext));
    destinationContext->thisThread = (thread_t)ksthread_self();
    destinationContext->isCrashedContext = true;
    destinationContext->isSignalContext = true;
    destinationContext->isStackOverflow = isStackOverflow(destinationContext);
    getThreadList(destinationContext);
    KSLOG_TRACE("Context retrieved.");
    return true;
}

void ksmc_addReservedThread(KSThread thread)
{
    int nextIndex = g_reservedThreadsCount;
    if(nextIndex > g_reservedThreadsMaxIndex)
    {
        KSLOG_ERROR("Too many reserved threads (%d). Max is %d", nextIndex, g_reservedThreadsMaxIndex);
        return;
    }
    g_reservedThreads[g_reservedThreadsCount++] = thread;
}

#if KSCRASH_HAS_THREADS_API
static inline bool isThreadInList(thread_t thread, KSThread* list, int listCount)
{
    for(int i = 0; i < listCount; i++)
    {
        if(list[i] == (KSThread)thread)
        {
            return true;
        }
    }
    return false;
}
#endif

void ksmc_suspendEnvironment()
{
#if KSCRASH_HAS_THREADS_API
    KSLOG_DEBUG("Suspending environment.");
    kern_return_t kr;
    const task_t thisTask = mach_task_self();
    const thread_t thisThread = (thread_t)ksthread_self();
    thread_act_array_t threads;
    mach_msg_type_number_t numThreads;
    
    if((kr = task_threads(thisTask, &threads, &numThreads)) != KERN_SUCCESS)
    {
        KSLOG_ERROR("task_threads: %s", mach_error_string(kr));
        return;
    }
    
    for(mach_msg_type_number_t i = 0; i < numThreads; i++)
    {
        thread_t thread = threads[i];
        if(thread != thisThread && !isThreadInList(thread, g_reservedThreads, g_reservedThreadsCount))
        {
            if((kr = thread_suspend(thread)) != KERN_SUCCESS)
            {
                // Record the error and keep going.
                KSLOG_ERROR("thread_suspend (%08x): %s", thread, mach_error_string(kr));
            }
        }
    }
    
    for(mach_msg_type_number_t i = 0; i < numThreads; i++)
    {
        mach_port_deallocate(thisTask, threads[i]);
    }
    vm_deallocate(thisTask, (vm_address_t)threads, sizeof(thread_t) * numThreads);
    
    KSLOG_DEBUG("Suspend complete.");
#endif
}

void ksmc_resumeEnvironment()
{
#if KSCRASH_HAS_THREADS_API
    KSLOG_DEBUG("Resuming environment.");
    kern_return_t kr;
    const task_t thisTask = mach_task_self();
    const thread_t thisThread = (thread_t)ksthread_self();
    thread_act_array_t threads;
    mach_msg_type_number_t numThreads;
    
    if((kr = task_threads(thisTask, &threads, &numThreads)) != KERN_SUCCESS)
    {
        KSLOG_ERROR("task_threads: %s", mach_error_string(kr));
        return;
    }
    
    for(mach_msg_type_number_t i = 0; i < numThreads; i++)
    {
        thread_t thread = threads[i];
        if(thread != thisThread && !isThreadInList(thread, g_reservedThreads, g_reservedThreadsCount))
        {
            if((kr = thread_resume(thread)) != KERN_SUCCESS)
            {
                // Record the error and keep going.
                KSLOG_ERROR("thread_resume (%08x): %s", thread, mach_error_string(kr));
            }
        }
    }
    
    for(mach_msg_type_number_t i = 0; i < numThreads; i++)
    {
        mach_port_deallocate(thisTask, threads[i]);
    }
    vm_deallocate(thisTask, (vm_address_t)threads, sizeof(thread_t) * numThreads);

    KSLOG_DEBUG("Resume complete.");
#endif
}

int ksmc_getThreadCount(const KSMachineContext* const context)
{
    return context->threadCount;
}

KSThread ksmc_getThreadAtIndex(const KSMachineContext* const context, int index)
{
    return context->allThreads[index];
    
}

int ksmc_indexOfThread(const KSMachineContext* const context, KSThread thread)
{
    KSLOG_TRACE("check thread vs %d threads", context->threadCount);
    for(int i = 0; i < (int)context->threadCount; i++)
    {
        KSLOG_TRACE("%d: %x vs %x", i, thread, context->allThreads[i]);
        if(context->allThreads[i] == thread)
        {
            return i;
        }
    }
    return -1;
}

bool ksmc_isCrashedContext(const KSMachineContext* const context)
{
    return context->isCrashedContext;
}

static inline bool isContextForCurrentThread(const KSMachineContext* const context)
{
    return context->isCurrentThread;
}

static inline bool isSignalContext(const KSMachineContext* const context)
{
    return context->isSignalContext;
}

bool ksmc_canHaveCPUState(const KSMachineContext* const context)
{
    return !isContextForCurrentThread(context) || isSignalContext(context);
}

bool ksmc_hasValidExceptionRegisters(const KSMachineContext* const context)
{
    return ksmc_canHaveCPUState(context) && ksmc_isCrashedContext(context);
}
