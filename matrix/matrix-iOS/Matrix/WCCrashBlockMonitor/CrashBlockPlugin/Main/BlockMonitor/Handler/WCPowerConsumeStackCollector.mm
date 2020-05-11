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

#import "WCPowerConsumeStackCollector.h"
#import <mach/mach_types.h>
#import <mach/mach_init.h>
#import <mach/thread_act.h>
#import <mach/task.h>
#import <mach/mach_port.h>
#import <mach/vm_map.h>
#import "KSStackCursor_SelfThread.h"
#import "KSSymbolicator.h"
#import "KSThread.h"
#import "MatrixLogDef.h"
#import "KSMachineContext.h"
#import "MatrixAsyncHook.h"
#import <pthread.h>
#import <os/lock.h>
#import <execinfo.h>

struct StackInfo
{
    uintptr_t **stack_matrix;
    int *trace_length_matrix;
};

// ============================================================================
#pragma mark - WCAddressFrame
// ============================================================================

#define SYMBOL_ADDRESS "object_address"
#define SYMBOL_NAME "object_name"
#define IMAGE_ADDRESS "image_address"
#define IMAGE_NAME "image_name"
#define INSTRUCTION_ADDRESS "instruction_address"
#define SAMPLE_COUNT "sample"
#define CHILE_FRAME "child"
#define MAX_STACK_TRACE_COUNT 100
#define FLOAT_THRESHOLD 0.000001
#define MAX_STACK_TRACE_IN_LOG 50

@interface WCAddressFrame ()
{
    uintptr_t m_symbolAddress;
    const char *m_symbolName;
    uintptr_t m_imageAddress;
    const char *m_imageName;
}

@property (nonatomic, assign) uintptr_t address;
@property (nonatomic, assign) uint32_t repeatCount;
@property (nonatomic, strong) NSMutableArray <WCAddressFrame *>*childAddressFrame;

- (void)symbolicate;

@end

@implementation WCAddressFrame

- (id)initWithAddress:(uintptr_t)address withRepeatCount:(uint32_t)count
{
    self = [super init];
    if (self) {
        _address = address;
        _repeatCount =  count;
        _childAddressFrame = [[NSMutableArray alloc] init];
        
        m_symbolAddress = 0;
        m_symbolName = "???";
        m_imageAddress = 0;
        m_imageName = "???";
    }
    return self;
}

- (void)addChildFrame:(WCAddressFrame *)addressFrame
{
    [_childAddressFrame addObject:addressFrame];
}

- (NSDictionary *)getInfoDict
{
    if (m_symbolName == NULL) {
        m_symbolName = "???";
    }
    if (m_imageName == NULL) {
        m_imageName = "???";
    }
    return @{@SYMBOL_ADDRESS : [NSNumber numberWithUnsignedLong:m_symbolAddress],
             @SYMBOL_NAME : [NSString stringWithUTF8String:m_symbolName],
             @IMAGE_ADDRESS : [NSNumber numberWithUnsignedLong:m_imageAddress],
             @IMAGE_NAME : [NSString stringWithUTF8String:m_imageName],
             @INSTRUCTION_ADDRESS : [NSNumber numberWithUnsignedInteger:self.address],
             @SAMPLE_COUNT : [NSNumber numberWithInt:self.repeatCount]};
}

- (void)symbolicate
{
    Dl_info symbolsBuffer;
    if(ksdl_dladdr(CALL_INSTRUCTION_FROM_RETURN_ADDRESS(self.address), &symbolsBuffer))
    {
        m_imageAddress = (uintptr_t)symbolsBuffer.dli_fbase;
        m_imageName = symbolsBuffer.dli_fname;
        m_symbolAddress = (uintptr_t)symbolsBuffer.dli_saddr;
        m_symbolName = symbolsBuffer.dli_sname;
    }
    
    if (_childAddressFrame == nil || [_childAddressFrame count] == 0) {
        return;
    }
    for (WCAddressFrame *addressFrame in _childAddressFrame) {
        [addressFrame symbolicate];
    }
}

- (WCAddressFrame *)tryFoundAddressFrameWithAddress:(uintptr_t)toFindAddress
{
    if (self.address == toFindAddress) {
        return self;
    } else {
        if (self.childAddressFrame == nil || [self.childAddressFrame count] == 0) {
            return nil;
        } else {
            for (WCAddressFrame *frame in self.childAddressFrame) {
                WCAddressFrame *foundFrame = [frame tryFoundAddressFrameWithAddress:toFindAddress];
                if (foundFrame != nil) {
                    return foundFrame;
                }
            }
            return nil;
        }
    }
}

@end

// ============================================================================
#pragma mark - WCStackTracePool
// ============================================================================


@interface WCStackTracePool ()
{
    uintptr_t **m_stackCyclePool;
    size_t *m_stackCount;
    float *m_stackCPU;
    uint64_t m_poolTailPoint;
    size_t m_maxStackCount;
}

/* conlusion */
@property (nonatomic, strong) NSMutableArray <WCAddressFrame *>*parentAddressFrame;

@end

@implementation WCStackTracePool

- (id)init
{
    return [self initWithMaxStackTraceCount:10];
}

- (id)initWithMaxStackTraceCount:(NSUInteger)maxStackTraceCount
{
    self = [super init];
    if (self) {
        
        m_maxStackCount = (size_t)maxStackTraceCount;
        
        size_t cycleArrayBytes = m_maxStackCount * sizeof(uintptr_t *);
        m_stackCyclePool = (uintptr_t **) malloc(cycleArrayBytes);
        if (m_stackCyclePool != NULL) {
            memset(m_stackCyclePool, 0, cycleArrayBytes);
        }
        size_t countArrayBytes = m_maxStackCount * sizeof(size_t);
        m_stackCount = (size_t *) malloc(countArrayBytes);
        if (m_stackCount != NULL) {
            memset(m_stackCount, 0, countArrayBytes);
        }
        size_t cpuArrayBytes = m_maxStackCount * sizeof(float);
        m_stackCPU = (float *) malloc(cpuArrayBytes);
        if (m_stackCPU != NULL) {
            memset(m_stackCPU, 0, cpuArrayBytes);
        }
        m_poolTailPoint = 0;
    }
    return self;
}

- (void)dealloc
{
    for (uint32_t i = 0; i < m_maxStackCount; i++) {
        if (m_stackCyclePool[i] != NULL) {
            free(m_stackCyclePool[i]);
            m_stackCyclePool[i] = NULL;
        }
    }
    if (m_stackCyclePool != NULL) {
        free(m_stackCyclePool);
        m_stackCyclePool = NULL;
    }
    if (m_stackCount != NULL) {
        free(m_stackCount);
        m_stackCount = NULL;
    }
    if (m_stackCPU != NULL) {
        free(m_stackCPU);
        m_stackCPU = NULL;
    }
}

- (void)addThreadStack:(uintptr_t *)stackArray andLength:(size_t)stackCount andCPU:(float)stackCPU
{
    if (stackArray == NULL) {
        return;
    }
    if (m_stackCyclePool == NULL || m_stackCount == NULL) {
        return;
    }
    if (stackCount == 0) {
        return;
    }
    if (m_stackCyclePool[m_poolTailPoint] != NULL) {
        free(m_stackCyclePool[m_poolTailPoint]);
    }
    m_stackCyclePool[m_poolTailPoint] = stackArray;
    m_stackCount[m_poolTailPoint] = stackCount;
    m_stackCPU[m_poolTailPoint] = stackCPU;
    
    m_poolTailPoint = (m_poolTailPoint + 1) % m_maxStackCount;
}

- (NSArray <NSDictionary *>*)makeCallTree
{
    _parentAddressFrame = nil;
    
    for (int i = 0; i < m_maxStackCount; i++) {
        uintptr_t *curStack = m_stackCyclePool[i];
        size_t curLength = m_stackCount[i];
        WCAddressFrame *curAddressFrame = [self p_getAddressFrameWithStackTraces:curStack length:curLength cpu:m_stackCPU[i]];
        [self p_addAddressFrame:curAddressFrame];
    }
    
    NSMutableArray <NSDictionary *>*addressDictArray = [[NSMutableArray alloc] init];
    
    if ([self.parentAddressFrame count] > 1) {
        [self.parentAddressFrame sortUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
            WCAddressFrame *frame1 = (WCAddressFrame *)obj1;
            WCAddressFrame *frame2 = (WCAddressFrame *)obj2;
            if (frame1.repeatCount > frame2.repeatCount) {
                return NSOrderedAscending;
            }
            return NSOrderedDescending;
        }];
    }
    
    for (int i = 0; i < [self.parentAddressFrame count] && i < 2; i++) {
        WCAddressFrame *addressFrame = self.parentAddressFrame[i];
        [addressFrame symbolicate];
        NSDictionary *curDict = [self p_getInfoDictFromAddressFrame:addressFrame];
        [addressDictArray addObject:curDict];
    }
    
    return [addressDictArray copy];
}

- (NSDictionary *)p_getInfoDictFromAddressFrame:(WCAddressFrame *)addressFrame
{
    NSMutableDictionary *currentInfoDict = [[addressFrame getInfoDict] mutableCopy];
    NSMutableArray <NSDictionary *> *childInfoDict = [[NSMutableArray alloc] init];
    
    if ([addressFrame.childAddressFrame count] > 1) {
        [addressFrame.childAddressFrame sortUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
            WCAddressFrame *frame1 = (WCAddressFrame *)obj1;
            WCAddressFrame *frame2 = (WCAddressFrame *)obj2;
            if (frame1.repeatCount > frame2.repeatCount) {
                return NSOrderedAscending;
            }
            return NSOrderedDescending;
        }];
    }
    
    for (WCAddressFrame *tmpCallFrame in addressFrame.childAddressFrame) {
        [childInfoDict addObject:[self p_getInfoDictFromAddressFrame:tmpCallFrame]];
    }
    
    if (childInfoDict != nil && [childInfoDict count] > 0) {
        [currentInfoDict setObject:[childInfoDict copy] forKey:@CHILE_FRAME];
    }
    return [currentInfoDict copy];
}

- (WCAddressFrame *)p_getAddressFrameWithStackTraces:(uintptr_t *)stackTrace length:(size_t)traceLength cpu:(float)stackCPU
{
    if (stackTrace == NULL || traceLength== 0) {
        return nil;
    }
    WCAddressFrame *headAddressFrame = nil;
    WCAddressFrame *currentParentFrame = nil;
    
    uint32_t repeatWeight = (uint32_t)(stackCPU / 5.);
    for (int i = 0; i < traceLength && i < MAX_STACK_TRACE_IN_LOG; i++) {
        uintptr_t address = stackTrace[i];
        WCAddressFrame *curFrame = [[WCAddressFrame alloc] initWithAddress:address withRepeatCount:repeatWeight];
        if (currentParentFrame == nil) {
            headAddressFrame = curFrame;
            currentParentFrame = curFrame;
        } else {
            [currentParentFrame addChildFrame:curFrame];
            currentParentFrame = curFrame;
        }
    }
    return headAddressFrame;
}

- (void)p_addAddressFrame:(WCAddressFrame *)addressFrame
{
    if (addressFrame == nil) {
        return;
    }
    if (_parentAddressFrame == nil) {
        _parentAddressFrame = [[NSMutableArray alloc] init];
    }
    if ([_parentAddressFrame count] == 0) {
        [_parentAddressFrame addObject:addressFrame];
    } else {
        WCAddressFrame *foundAddressFrame = nil;
        for (WCAddressFrame *tmpFrame in _parentAddressFrame) {
            foundAddressFrame = [tmpFrame tryFoundAddressFrameWithAddress:addressFrame.address];
            if (foundAddressFrame != nil) {
                break;
            }
        }
        if (foundAddressFrame == nil) {
            [_parentAddressFrame addObject:addressFrame];
        } else {
            [self p_mergeAddressFrame:foundAddressFrame with:addressFrame];
        }
    }
}

- (void)p_mergeAddressFrame:(WCAddressFrame *)mainFrame with:(WCAddressFrame *)mergedFrame
{
    if (mainFrame.address != mergedFrame.address) {
        assert(0);
    }
    mainFrame.repeatCount += mergedFrame.repeatCount;
    
    if (mainFrame.childAddressFrame == nil || [mainFrame.childAddressFrame count] == 0) {
        mainFrame.childAddressFrame = mergedFrame.childAddressFrame;
        return; // full with mergedFrame.childeAddressFrame
    }
    
    [self p_mergedAddressFrameArray:mainFrame.childAddressFrame with:mergedFrame.childAddressFrame];
}

- (void)p_mergedAddressFrameArray:(NSMutableArray <WCAddressFrame *>*)mainFrameArray with:(NSMutableArray <WCAddressFrame *>*)mergedFrameArray
{
    if (mergedFrameArray == nil || [mergedFrameArray count] == 0) {
        return;
    }
    NSMutableArray <WCAddressFrame *>*notFoundFrame = [NSMutableArray array];
    for (WCAddressFrame *mergedFrame in mergedFrameArray) {
        BOOL bFound = NO;
        for (WCAddressFrame *mainFrame in mainFrameArray) {
            if (mergedFrame.address == mainFrame.address) {
                bFound = YES;
                [self p_mergeAddressFrame:mainFrame with:mergedFrame];
                break;
            }
        }
        if (bFound == NO) {
            [notFoundFrame addObject:mergedFrame];
        }
    }
    [mainFrameArray addObjectsFromArray:notFoundFrame];
}

@end

// ============================================================================
#pragma mark - WCCpuStackFrame
// ============================================================================
@interface WCCpuStackFrame : NSObject

@property (nonatomic, assign) thread_t cpu_thread;
@property (nonatomic, assign) float cpu_value;

- (id)initWithThread:(thread_t)cpu_thread andCpuValue:(float)cpu_value;

@end

@implementation WCCpuStackFrame

- (id)initWithThread:(thread_t)cpu_thread andCpuValue:(float)cpu_value
{
    self = [super init];
    if (self) {
        self.cpu_thread = cpu_thread;
        self.cpu_value = cpu_value;
    }
    return self;
}

@end


// ============================================================================
#pragma mark - WCPowerConsumeStackCollector
// ============================================================================

static float g_kGetPowerStackCPULimit = 80.;
static KSStackCursor **g_cpuHighThreadArray = NULL;
static int g_cpuHighThreadNumber = 0;
static float *g_cpuHighThreadValueArray = NULL;

@interface WCPowerConsumeStackCollector ()

@property (nonatomic, strong) WCStackTracePool *stackTracePool;

@end

@implementation WCPowerConsumeStackCollector

- (id)initWithCPULimit:(float)cpuLimit
{
    self = [super init];
    if (self) {
        g_kGetPowerStackCPULimit = cpuLimit;
        _stackTracePool = [[WCStackTracePool alloc] initWithMaxStackTraceCount:MAX_STACK_TRACE_COUNT];
    }
    return self;
}

// ============================================================================
#pragma mark - Power Consume Block
// ============================================================================

- (void)makeConclusion
{
    WCStackTracePool *handlePool = _stackTracePool;
    _stackTracePool = [[WCStackTracePool alloc] initWithMaxStackTraceCount:MAX_STACK_TRACE_COUNT];
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSArray <NSDictionary *>*stackTree = [handlePool makeCallTree];
        if (self.delegate != nil && [self.delegate respondsToSelector:@selector(powerConsumeStackCollectorConclude:)]) {
            [self.delegate powerConsumeStackCollectorConclude:stackTree];
        }
    });
}

- (float)getCPUUsageAndPowerConsumeStack
{
    mach_msg_type_number_t thread_count;
    NSMutableArray <WCCpuStackFrame *> *cost_cpu_thread_array = [[NSMutableArray alloc] init];

    float result = [self getTotCpuWithCostCpuThreadArray:&cost_cpu_thread_array andThreadCount:&thread_count];
    
    if (fabs(result + 1.0) < FLOAT_THRESHOLD) {
        return -1.0;
    }
    
    thread_t *cost_cpu_thread_list = (thread_t *)malloc(sizeof(thread_t) * thread_count);
    float *cost_cpu_value_list = (float *)malloc(sizeof(float) * thread_count);
    mach_msg_type_number_t cost_cpu_thread_count = 0;
    
    for (int i = 0; i < [cost_cpu_thread_array count]; i++) {
        cost_cpu_thread_list[i] = cost_cpu_thread_array[i].cpu_thread;
        cost_cpu_value_list[i] = cost_cpu_thread_array[i].cpu_value;
        cost_cpu_thread_count++;
    }
    
    // backtrace the thread with power consuming stack
    if (result > g_kGetPowerStackCPULimit && cost_cpu_thread_count > 0) {
        StackInfo stackInfo = [self getStackInfoWithThreadCount:cost_cpu_thread_count
                                              costCpuThreadList:cost_cpu_thread_list
                                               costCpuValueList:cost_cpu_value_list];
        
        uintptr_t **stack_matrix = stackInfo.stack_matrix;
        int *trace_length_matrix = stackInfo.trace_length_matrix;
        
        if (stack_matrix != NULL && trace_length_matrix != NULL) {
            for (int i = 0; i < cost_cpu_thread_count; i++) {
                if (stack_matrix[i] != NULL) {
                    [_stackTracePool addThreadStack:stack_matrix[i]
                                          andLength:(size_t)trace_length_matrix[i]
                                             andCPU:cost_cpu_value_list[i]];
                }
            }
            free(stack_matrix);
            free(trace_length_matrix);
        }
    }
    
    free(cost_cpu_thread_list);
    free(cost_cpu_value_list);
    return result;
}

// ============================================================================
#pragma mark - CPU High Block
// ============================================================================

- (BOOL)isCPUHighBlock
{
    if (fabs([self getCPUUsageAndCPUBlockStack] + 1) < FLOAT_THRESHOLD) {
        return NO;
    }
    return YES;
}

- (float)getCPUUsageAndCPUBlockStack
{
    mach_msg_type_number_t thread_count;
    NSMutableArray <WCCpuStackFrame *> *cost_cpu_thread_array = [[NSMutableArray alloc] init];
    
    float result = [self getTotCpuWithCostCpuThreadArray:&cost_cpu_thread_array andThreadCount:&thread_count];
    
    if (fabs(result + 1.0) < FLOAT_THRESHOLD) {
        return -1.0;
    }
    
    // sort the thread array according to its cost value when cpu high block
    [cost_cpu_thread_array sortUsingComparator:^NSComparisonResult(id  _Nonnull obj1, id  _Nonnull obj2) {
        WCCpuStackFrame *frame1 = (WCCpuStackFrame *)obj1;
        WCCpuStackFrame *frame2 = (WCCpuStackFrame *)obj2;
        if (frame1.cpu_value > frame2.cpu_value) {
            return NSOrderedAscending;
        }
        return NSOrderedDescending;
    }];
    
    thread_t *cost_cpu_thread_list = (thread_t *)malloc(sizeof(thread_t) * thread_count);
    float *cost_cpu_value_list = (float *)malloc(sizeof(float) * thread_count);
    mach_msg_type_number_t cost_cpu_thread_count = 0;
    
    // for cpu high block: the maximum number of thread is 3
    for (int i = 0; i < [cost_cpu_thread_array count] && i < 3; i++) {
        cost_cpu_thread_list[i] = cost_cpu_thread_array[i].cpu_thread;
        cost_cpu_value_list[i] = cost_cpu_thread_array[i].cpu_value;
        cost_cpu_thread_count++;
    }
    
    // backtrace the thread with power consuming stack
    if (result > g_kGetPowerStackCPULimit && cost_cpu_thread_count > 0) {
        StackInfo stackInfo = [self getStackInfoWithThreadCount:cost_cpu_thread_count
                                              costCpuThreadList:cost_cpu_thread_list
                                               costCpuValueList:cost_cpu_value_list];
        
        uintptr_t **stack_matrix = stackInfo.stack_matrix;
        int *trace_length_matrix = stackInfo.trace_length_matrix;
        
        if (stack_matrix != NULL && trace_length_matrix != NULL) {
            g_cpuHighThreadArray = (KSStackCursor **)malloc(sizeof(KSStackCursor *) * (int)cost_cpu_thread_count);
            g_cpuHighThreadNumber = (int) cost_cpu_thread_count;
            g_cpuHighThreadValueArray = (float *)malloc(sizeof(float) * (int)cost_cpu_thread_count);
            
            for (int i = 0; i < cost_cpu_thread_count; i++) {
                if (stack_matrix[i] != NULL) {
                    g_cpuHighThreadArray[i] = (KSStackCursor *)malloc(sizeof(KSStackCursor));
                    kssc_initWithBacktrace(g_cpuHighThreadArray[i], stack_matrix[i], trace_length_matrix[i], 0);
                    g_cpuHighThreadValueArray[i] = cost_cpu_value_list[i];
                }
            }
            
            free(stack_matrix);
            free(trace_length_matrix);
        }
    }
    
    free(cost_cpu_thread_list);
    free(cost_cpu_value_list);
    return result;
}

- (int)getCurrentCpuHighStackNumber
{
    return g_cpuHighThreadNumber;
}

- (KSStackCursor **)getCPUStackCursor
{
    return g_cpuHighThreadArray;
}

- (float *)getCpuHighThreadValueArray
{
    return g_cpuHighThreadValueArray;
}

// ============================================================================
#pragma mark - Class Function
// ============================================================================

+ (float)getCurrentCPUUsage
{
    kern_return_t kr;
    task_info_data_t tinfo;
    mach_msg_type_number_t task_info_count;
    
    task_info_count = TASK_INFO_MAX;
    kr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t) tinfo, &task_info_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    thread_array_t thread_list;
    mach_msg_type_number_t thread_count;
    kr = task_threads(mach_task_self(), &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    float tot_cpu = 0;
    
    for (int j = 0; j < thread_count; j++) {
        thread_info_data_t thinfo;
        mach_msg_type_number_t thread_info_count = THREAD_INFO_MAX;
        kr = thread_info(thread_list[j], THREAD_BASIC_INFO, (thread_info_t)thinfo, &thread_info_count);
        if (kr != KERN_SUCCESS) {
            return -1;
        }
        thread_basic_info_t basic_info_th = (thread_basic_info_t)thinfo;
        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            tot_cpu = tot_cpu + basic_info_th->cpu_usage / (float) TH_USAGE_SCALE * 100.0;
        }
    }
    
    kr = vm_deallocate(mach_task_self(), (vm_offset_t) thread_list, thread_count * sizeof(thread_t));
    return tot_cpu;
}

// ============================================================================
#pragma mark - Helper Function
// ============================================================================

- (float)getTotCpuWithCostCpuThreadArray:(NSMutableArray <WCCpuStackFrame *> **)cost_cpu_thread_array andThreadCount:(mach_msg_type_number_t *)thread_count
{
    // variable declarations
    kern_return_t kr;
    task_info_data_t tinfo;
    mach_msg_type_number_t task_info_count;
    
    task_info_count = TASK_INFO_MAX;
    kr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)tinfo, &task_info_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    thread_array_t thread_list;
    
    // get threads in the task
    kr = task_threads(mach_task_self(), &thread_list, thread_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    float tot_cpu = 0.0;
    const thread_t thisThread = (thread_t)ksthread_self();
    
    // get cost cpu threads and stored in array
    for (int j = 0; j < *thread_count; j++) {
        thread_t current_thread = thread_list[j];
        
        thread_info_data_t thinfo;
        mach_msg_type_number_t thread_info_count = THREAD_INFO_MAX;
        kr = thread_info(current_thread, THREAD_BASIC_INFO, (thread_info_t)thinfo, &thread_info_count);
        if (kr != KERN_SUCCESS) {
            return -1;
        }
        
        thread_basic_info_t basic_info_th = (thread_basic_info_t)thinfo;
        float cur_cpu = 0;
        long cur_sec = 0;
        long cur_usec = 0;
        
        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            cur_sec = basic_info_th->user_time.seconds + basic_info_th->system_time.seconds;
            cur_usec = basic_info_th->system_time.microseconds + basic_info_th->system_time.microseconds;
            cur_cpu = basic_info_th->cpu_usage / (float) TH_USAGE_SCALE * 100.0;
            tot_cpu = tot_cpu + cur_cpu;
        }
        
        if (cur_cpu > 5. && current_thread != thisThread && *cost_cpu_thread_array != NULL) {
            WCCpuStackFrame *cpu_stack_frame = [[WCCpuStackFrame alloc] initWithThread:current_thread andCpuValue:cur_cpu];
            [*cost_cpu_thread_array addObject:cpu_stack_frame];
        }

    }
    kr = vm_deallocate(mach_task_self(), (vm_offset_t) thread_list, *thread_count * sizeof(thread_t));
    return tot_cpu;
}

- (StackInfo)getStackInfoWithThreadCount:(mach_msg_type_number_t)cost_cpu_thread_count
                       costCpuThreadList:(thread_t *)cost_cpu_thread_list
                        costCpuValueList:(float *)cost_cpu_value_list
{
    struct StackInfo result;
    do { // get power-consuming stack
        size_t maxEntries = 100;
        int *trace_length_matrix = (int *)malloc(sizeof(int) * cost_cpu_thread_count);
        if (trace_length_matrix == NULL) {
            break;
        }
        uintptr_t **stack_matrix = (uintptr_t **)malloc(sizeof(uintptr_t *) * cost_cpu_thread_count);
        if (stack_matrix == NULL) {
            break;
        }
        BOOL have_null = NO;
        for (int i = 0; i < cost_cpu_thread_count; i++) {
            
            // the alloc size should contain async thread
            stack_matrix[i] = (uintptr_t *)malloc(sizeof(uintptr_t) * maxEntries * 2);
            if (stack_matrix == NULL) {
                have_null = YES;
            }
        }
        if (have_null) {
            for (int i = 0; i < cost_cpu_thread_count; i++) {
                if (stack_matrix[i] != NULL) {
                    free(stack_matrix[i]);
                }
            }
            free(stack_matrix);
            break;
        }
        
        uintptr_t **async_stack_trace_array = (uintptr_t **)malloc(sizeof(uintptr_t *) * cost_cpu_thread_count);
        int *async_stack_trace_array_count = (int *)malloc(sizeof(int) * cost_cpu_thread_count);
        
        // get the origin async stack from MatrixAsyncHook
        MatrixAsyncHook *asyncHook = [MatrixAsyncHook sharedInstance];
        NSDictionary *asyncThreadStackDict = [asyncHook getAsyncOriginThreadDict];
        
        for (int i = 0; i < cost_cpu_thread_count; i++) {
            thread_t current_thread = cost_cpu_thread_list[i];
            NSArray *asyncStackTrace = [asyncThreadStackDict objectForKey:[[NSNumber alloc] initWithInt:current_thread]];
            
            if (asyncStackTrace != nil && [asyncStackTrace count] != 0) {
                async_stack_trace_array[i] = (uintptr_t *)malloc(sizeof(uintptr_t) * [asyncStackTrace count]);
                async_stack_trace_array_count[i] = (int)[asyncStackTrace count];
                for (int j = 0; j < [asyncStackTrace count]; j++) {
                    async_stack_trace_array[i][j] = [asyncStackTrace[j] unsignedLongValue];
                }
            } else {
                async_stack_trace_array[i] = NULL;
                async_stack_trace_array_count[i] = 0;
            }
        }
        
        ksmc_suspendEnvironment();
        for (int i = 0; i < cost_cpu_thread_count; i++) {
            thread_t current_thread = cost_cpu_thread_list[i];
            uintptr_t backtrace_buffer[maxEntries];
            
            trace_length_matrix[i] = kssc_backtraceCurrentThread(current_thread, backtrace_buffer, (int)maxEntries);
   
            int j = 0;
            for (; j < trace_length_matrix[i]; j++) {
                stack_matrix[i][j] = backtrace_buffer[j];
            }
            
            if (async_stack_trace_array_count[i] != 0 && async_stack_trace_array[i] != NULL) {
                trace_length_matrix[i] += async_stack_trace_array_count[i];
                for (int k = 0; k < async_stack_trace_array_count[i]; j++, k++) {
                    stack_matrix[i][j] = async_stack_trace_array[i][k];
                }
            }
        }
        ksmc_resumeEnvironment();
        
        for (int i = 0; i < cost_cpu_thread_count; i++) {
            if (async_stack_trace_array[i] != NULL && async_stack_trace_array_count[i] != 0) {
                free(async_stack_trace_array[i]);
            }
        }
        free(async_stack_trace_array);
        free(async_stack_trace_array_count);
        
        result = {stack_matrix, trace_length_matrix};
    } while (0);
    return result;
}

@end
