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
ks_rebind_symbols((struct ks_rebinding[2]){{#func, WRAP(func), (void *)&ORIFUNC(func)}}, 1);

#define NEW_CONTEXT_WORK_RECORD(context,work) newContextRecordAsyncTrace(context, work),replaceFuncRecordAsyncTrace

#import "MatrixAsyncHook.h"
#import "KSCrash_fishhook.h"
#import <objc/runtime.h>
#import "KSThread.h"
#import "KSStackCursor_SelfThread.h"
#import <mach/mach_port.h>
#import <execinfo.h>
#import <pthread.h>
#import "MatrixLogDef.h"

static NSMutableDictionary *asyncOriginThreadDict;
static pthread_mutex_t m_threadLock;

// ============================================================================
#pragma mark - hook dispatch func
// ============================================================================
typedef struct AsyncStackTrace {
    void **backTrace;
    size_t size;
} AsyncStackTrace;

static int MaxBacktraceLimit = 100;

__attribute__((always_inline)) AsyncStackTrace getCurAsyncStackTrace(void)
{
    AsyncStackTrace asyncStackTrace;
    asyncStackTrace.backTrace = NULL;
    asyncStackTrace.size = 0;
    
    void **backTracePtr = (void **)malloc(sizeof(void*) * MaxBacktraceLimit);
    
    // TODO: the waring informs the condition will always be true
    if (backTracePtr != NULL) {
        size_t size = backtrace(backTracePtr, MaxBacktraceLimit);
        asyncStackTrace.backTrace = backTracePtr;
        asyncStackTrace.size = size;
    }
    return asyncStackTrace;
}

static inline dispatch_block_t blockRecordAsyncTrace(dispatch_block_t block)
{
    // 1. get origin stack
    AsyncStackTrace stackTrace = getCurAsyncStackTrace();
    NSMutableArray *stackArray = [[NSMutableArray alloc] init];
    for (int i = 0; i < stackTrace.size; i++) {
        NSNumber *temp =[NSNumber numberWithUnsignedLong:(unsigned long)stackTrace.backTrace[i]];
        [stackArray addObject:temp];
    }
    free(stackTrace.backTrace);
    stackTrace.backTrace = NULL;
    
    // 2. execute the block
    dispatch_block_t newBlock = ^() {
        pthread_mutex_lock(&m_threadLock);
        thread_t current_thread = (thread_t)ksthread_self();
        NSNumber *key = [[NSNumber alloc] initWithInt:current_thread];
        [asyncOriginThreadDict setObject:stackArray forKey:key];
        pthread_mutex_unlock(&m_threadLock);
        
        block();
        
        pthread_mutex_lock(&m_threadLock);
        if (key != nil && [asyncOriginThreadDict objectForKey:key] != nil) {
            [asyncOriginThreadDict removeObjectForKey:key];
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
    AsyncStackTrace asyncStackTrace;
} AsyncRecord;

static inline void* newContextRecordAsyncTrace(void *_Nullable context, dispatch_function_t work) {
    AsyncRecord *record = (AsyncRecord *)calloc(sizeof(AsyncRecord),1);
    record->context = context;
    record->function = work;
    
    // get origin stack
    AsyncStackTrace asyncStackTrace = getCurAsyncStackTrace();
    record->asyncStackTrace = asyncStackTrace;
    
    return record;
}

static void replaceFuncRecordAsyncTrace(void *_Nullable context) {
    AsyncRecord *record = (AsyncRecord *)context;
    AsyncStackTrace stackTrace = record->asyncStackTrace;
    dispatch_function_t function = record->function;
    NSCAssert(function, @"oriFunc is nil");
    
    if (function != NULL) {
        // 1. stack -> array
        NSMutableArray *stackArray = [[NSMutableArray alloc] init];
        for (int i = 0; i < stackTrace.size; i++) {
            NSNumber *temp =[NSNumber numberWithUnsignedLong:(unsigned long)stackTrace.backTrace[i]];
            [stackArray addObject:temp];
        }
        free(stackTrace.backTrace);
        stackTrace.backTrace = NULL;
        
        // 2. associate the stack array with the thread
        pthread_mutex_lock(&m_threadLock);
        thread_t current_thread = (thread_t)ksthread_self();
        NSNumber *key = [[NSNumber alloc] initWithInt:current_thread];
        [asyncOriginThreadDict setObject:stackArray forKey:key];
        pthread_mutex_unlock(&m_threadLock);
        
        // 3. execute the function
        function(record->context);
        
        pthread_mutex_lock(&m_threadLock);
        if (key != nil && [asyncOriginThreadDict objectForKey:key] != nil) {
            [asyncOriginThreadDict removeObjectForKey:key];
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

// ============================================================================
#pragma mark - hook perform selector
// ============================================================================
@interface AsyncRecordParam : NSObject

@property (nonatomic, strong) id arg;
@property (nonatomic, assign) SEL aSelector;
@property (nonatomic, strong) NSArray *stackArray;

@end

@implementation AsyncRecordParam

- (instancetype)initWithSelector:(SEL)aSelector ar:(id)arg
{
    if (self = [super init]) {
        self.aSelector = aSelector;
        self.arg = arg;
    }
    return self;
}

@end

@interface NSObject (AsyncHookNSThreadPerformAdditions)

@end

@implementation NSObject (AsyncHookNSThreadPerformAdditions)

- (void)hookPerformSelector:(SEL)aSelector
                   onThread:(NSThread *)thr
                 withObject:(nullable id)arg
              waitUntilDone:(BOOL)wait
                      modes:(nullable NSArray<NSString *> *)array
{
    if (!wait) {
        
        // 1. get origin async stack trace
        AsyncStackTrace stackTrace = getCurAsyncStackTrace();
        NSMutableArray *stackArray = [[NSMutableArray alloc] init];
        for (int i = 0; i < stackTrace.size; i++)
        {
            NSNumber *temp =[NSNumber numberWithUnsignedLong:(unsigned long)stackTrace.backTrace[i]];
            [stackArray addObject:temp];
        }
        
        free(stackTrace.backTrace);
        stackTrace.backTrace = NULL;
        
        AsyncRecordParam *param = [[AsyncRecordParam alloc] initWithSelector:aSelector ar:arg];
        param.stackArray = [[NSArray alloc] initWithArray:stackArray];
        
        [self hookPerformSelector:@selector(performWithAsyncTrace:)
                         onThread:thr
                       withObject:param
                    waitUntilDone:wait
                            modes:array];
    } else {
        [self hookPerformSelector:aSelector
                         onThread:thr
                       withObject:arg
                    waitUntilDone:wait
                            modes:array];
    }
}

- (void)performWithAsyncTrace:(AsyncRecordParam *)param
{
    // record the async stack trace
    pthread_mutex_lock(&m_threadLock);
    thread_t current_thread = (thread_t)ksthread_self();
    NSNumber *key = [[NSNumber alloc] initWithInt:current_thread];
    [asyncOriginThreadDict setObject:param.stackArray forKey:key];
    pthread_mutex_unlock(&m_threadLock);
    
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [self performSelector:param.aSelector withObject:param.arg];
}

@end


// ============================================================================
#pragma mark - MatrixAsyncHook
// ============================================================================

@interface MatrixAsyncHook()

@end

@implementation MatrixAsyncHook

+ (instancetype)sharedInstance
{
    static MatrixAsyncHook *hookInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        hookInstance = [[self alloc] init];
        
    });
    return hookInstance;
}

- (id)init
{
    self = [super init];
    if (self) {
        asyncOriginThreadDict = [[NSMutableDictionary alloc] init];
        pthread_mutex_init(&m_threadLock, NULL);
    }
    return self;
}

- (void)beginHook
{
    // 1. hook dispatch
    BEGIN_HOOK(dispatch_async);
    BEGIN_HOOK(dispatch_after);
    BEGIN_HOOK(dispatch_barrier_async);
    
    BEGIN_HOOK(dispatch_async_f);
    BEGIN_HOOK(dispatch_after_f);
    BEGIN_HOOK(dispatch_barrier_async_f);
    
}

- (NSDictionary *)getAsyncOriginThreadDict
{
    pthread_mutex_lock(&m_threadLock);
    NSDictionary *tmp = [asyncOriginThreadDict copy];
    pthread_mutex_unlock(&m_threadLock);
    return tmp;
}

@end
