/*
* Tencent is pleased to support the open source community by making wechat-matrix available.
* Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#define WRAP(x) warp_##x
#define ORIFUNC(func) orig_##func
#define HOOK_FUNC(ret_type, func, ...) \
ret_type func(__VA_ARGS__); \
static ret_type WRAP(func)(__VA_ARGS__); \
static ret_type (*ORIFUNC(func))(__VA_ARGS__); \
ret_type WRAP(func)(__VA_ARGS__) {

#define BEGIN_HOOK(func) \
ks_rebind_symbols((struct ks_rebinding[1]){{#func, (void *)WRAP(func), (void **)&ORIFUNC(func)}}, 1);

#define NEW_CONTEXT_WORK_RECORD(context,work) newContextRecordAsyncTrace(context, work),replaceFuncRecordAsyncTrace

#import "MatrixAsyncHook.h"
#import "KSCrash_fishhook.h"
#import <objc/runtime.h>
#import "KSStackCursor_SelfThread.h"
#import <mach/mach_port.h>
#import <execinfo.h>
#import <pthread.h>
#include <map>


static int MaxBacktraceLimit = 100;
static pthread_mutex_t m_threadLock;
[[clang::no_destroy]] static std::map<mach_port_t, AsyncStackTrace*> g_asyncStackMap;

__attribute__((always_inline)) AsyncStackTrace* getCurAsyncStackTrace(void)
{
    AsyncStackTrace *asyncStackTrace = (AsyncStackTrace *)malloc(sizeof(AsyncStackTrace));
    asyncStackTrace->backTrace = NULL;
    asyncStackTrace->size = 0;
    
    void **backTracePtr = (void **)malloc(sizeof(void*) * MaxBacktraceLimit);

    if (backTracePtr != NULL) {
        size_t size = backtrace(backTracePtr, MaxBacktraceLimit);
        asyncStackTrace->backTrace = backTracePtr;
        asyncStackTrace->size = size;
    }
    return asyncStackTrace;
}

// ============================================================================
#pragma mark - hook dispatch func
// ============================================================================

static dispatch_block_t blockRecordAsyncTrace(dispatch_block_t block)
{
    // 1. get origin stack
    __block AsyncStackTrace* stackTrace = getCurAsyncStackTrace();
    
    // 2. execute the block
    dispatch_block_t newBlock = ^() {
        pthread_mutex_lock(&m_threadLock);
        mach_port_t current_thread = (mach_port_t)pthread_mach_thread_np(pthread_self());
        if (stackTrace && stackTrace->backTrace) {
            g_asyncStackMap[current_thread] = stackTrace;
        }
        pthread_mutex_unlock(&m_threadLock);
        
        block();
        
        pthread_mutex_lock(&m_threadLock);
        if (stackTrace) {
            if (stackTrace->backTrace) {
                free(stackTrace->backTrace);
                stackTrace->backTrace = NULL;
            }
            free(stackTrace);
            stackTrace = NULL;
        }
        std::map<mach_port_t, AsyncStackTrace*>::iterator it;
        it = g_asyncStackMap.find(current_thread);
        if (it != g_asyncStackMap.end()) {
            g_asyncStackMap.erase(it);
        }
        pthread_mutex_unlock(&m_threadLock);
    };
    
    // 3. return the new block
    return newBlock;
}

HOOK_FUNC(void, dispatch_async, dispatch_queue_t queue, dispatch_block_t block)
orig_dispatch_async(queue,blockRecordAsyncTrace(block));
}

HOOK_FUNC(void, dispatch_after, dispatch_time_t when, dispatch_queue_t queue, dispatch_block_t block)
orig_dispatch_after(when,queue,blockRecordAsyncTrace(block));
}

HOOK_FUNC(void, dispatch_barrier_async, dispatch_queue_t queue, dispatch_block_t block)
orig_dispatch_barrier_async(queue,blockRecordAsyncTrace(block));
}

// ============================================================================
#pragma mark - hook dispatch_xxx_f func
// ============================================================================
typedef struct AsyncRecord {
    void *_Nullable context;
    dispatch_function_t function;
    AsyncStackTrace *asyncStackTrace;
} AsyncRecord;

static void* newContextRecordAsyncTrace(void *_Nullable context, dispatch_function_t work) {
    AsyncRecord *record = (AsyncRecord *)calloc(sizeof(AsyncRecord),1);
    record->context = context;
    record->function = work;
    
    // get origin stack
    AsyncStackTrace *asyncStackTrace = getCurAsyncStackTrace();
    record->asyncStackTrace = asyncStackTrace;
    
    return record;
}

static void replaceFuncRecordAsyncTrace(void *_Nullable context) {
    AsyncRecord *record = (AsyncRecord *)context;
    AsyncStackTrace *stackTrace = record->asyncStackTrace;
    dispatch_function_t function = record->function;
    NSCAssert(function, @"oriFunc is nil");
    
    if (function != NULL) {
        
        // 1. associate the stack array with the thread
        pthread_mutex_lock(&m_threadLock);
        mach_port_t current_thread = (mach_port_t)pthread_mach_thread_np(pthread_self());
        if (stackTrace && stackTrace->backTrace) {
            g_asyncStackMap[current_thread] = stackTrace;
        }
        pthread_mutex_unlock(&m_threadLock);
        
        // 2. execute the function
        function(record->context);
        
        pthread_mutex_lock(&m_threadLock);
        if (stackTrace) {
            if (stackTrace->backTrace) {
                free(stackTrace->backTrace);
                stackTrace->backTrace = NULL;
            }
            free(stackTrace);
            stackTrace = NULL;
        }
        std::map<mach_port_t, AsyncStackTrace*>::iterator it;
        it = g_asyncStackMap.find(current_thread);
        if (it != g_asyncStackMap.end()) {
            g_asyncStackMap.erase(it);
        }
        pthread_mutex_unlock(&m_threadLock);
    }
    free(record);
}

HOOK_FUNC(void, dispatch_async_f, dispatch_queue_t queue, void *_Nullable context, dispatch_function_t work)
orig_dispatch_async_f(queue,NEW_CONTEXT_WORK_RECORD(context,work));
}

HOOK_FUNC(void, dispatch_after_f, dispatch_time_t when, dispatch_queue_t queue, void *_Nullable context, dispatch_function_t work)
orig_dispatch_after_f(when,queue,NEW_CONTEXT_WORK_RECORD(context,work));
}

HOOK_FUNC(void, dispatch_barrier_async_f, dispatch_queue_t queue, void *_Nullable context, dispatch_function_t work)
orig_dispatch_barrier_async_f(queue,NEW_CONTEXT_WORK_RECORD(context,work));
}

void initObjects() {
    pthread_mutex_init(&m_threadLock, NULL);
}

void beginHookDispatch() {
    initObjects();
    
    BEGIN_HOOK(dispatch_async);
    BEGIN_HOOK(dispatch_after);
    BEGIN_HOOK(dispatch_barrier_async);
    BEGIN_HOOK(dispatch_async_f);
    BEGIN_HOOK(dispatch_after_f);
    BEGIN_HOOK(dispatch_barrier_async_f);
}

AsyncStackTrace* getAsyncStack(mach_port_t thread) {
    pthread_mutex_lock(&m_threadLock);
    AsyncStackTrace *stackInfo = (AsyncStackTrace *)malloc(sizeof(AsyncStackTrace));
    if (stackInfo != NULL) {
        std::map<mach_port_t, AsyncStackTrace*>::iterator it;
        it = g_asyncStackMap.find(thread);
        if (it != g_asyncStackMap.end()) {
            AsyncStackTrace *oriStackInfo = g_asyncStackMap[thread];
            stackInfo->size = oriStackInfo->size;
            void **backTracePtr = (void **)malloc(sizeof(void*) * oriStackInfo->size);
            if (backTracePtr) {
                for (int i = 0; i < oriStackInfo->size; i++) {
                    backTracePtr[i] = oriStackInfo->backTrace[i];
                }
                stackInfo->backTrace = backTracePtr;
            } else {
                free(stackInfo);
                stackInfo = NULL;
            }
        } else {
            free(stackInfo);
            stackInfo = NULL;
        }
    }
    pthread_mutex_unlock(&m_threadLock);
    return stackInfo;
}
