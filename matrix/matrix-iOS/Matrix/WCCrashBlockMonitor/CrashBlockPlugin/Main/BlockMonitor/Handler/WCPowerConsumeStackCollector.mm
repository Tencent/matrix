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
#import "KSThread.h"
#import "MatrixLogDef.h"
#import "KSMachineContext.h"
#import <pthread.h>
#import <os/lock.h>
#import <execinfo.h>
#import "WCMatrixModel.h"

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif

struct StackInfo {
    uintptr_t **stack_matrix;
    int *trace_length_matrix;
};

// ============================================================================
#pragma mark - WCAddressFrame
// ============================================================================

#define CHILE_FRAME "child"
#define MAX_STACK_TRACE_COUNT 100
#define FLOAT_THRESHOLD 0.000001
#define MAX_STACK_TRACE_IN_LOG 50

// ============================================================================
#pragma mark - WCStackTracePool
// ============================================================================

@interface WCStackTracePool () {
    uintptr_t **m_stackCyclePool;
    size_t *m_stackCount;
    float *m_stackCPU;
    BOOL *m_stackInBackground;
    uint64_t m_poolTailPoint;
    size_t m_maxStackCount;
}

/* conlusion */
@property (nonatomic, strong) NSMutableArray<WCAddressFrame *> *parentAddressFrame;

@end

@implementation WCStackTracePool

- (id)init {
    return [self initWithMaxStackTraceCount:10];
}

- (id)initWithMaxStackTraceCount:(NSUInteger)maxStackTraceCount {
    self = [super init];
    if (self) {
        m_maxStackCount = (size_t)maxStackTraceCount;

        size_t cycleArrayBytes = m_maxStackCount * sizeof(uintptr_t *);
        m_stackCyclePool = (uintptr_t **)malloc(cycleArrayBytes);
        if (m_stackCyclePool != NULL) {
            memset(m_stackCyclePool, 0, cycleArrayBytes);
        }
        size_t countArrayBytes = m_maxStackCount * sizeof(size_t);
        m_stackCount = (size_t *)malloc(countArrayBytes);
        if (m_stackCount != NULL) {
            memset(m_stackCount, 0, countArrayBytes);
        }
        size_t cpuArrayBytes = m_maxStackCount * sizeof(float);
        m_stackCPU = (float *)malloc(cpuArrayBytes);
        if (m_stackCPU != NULL) {
            memset(m_stackCPU, 0, cpuArrayBytes);
        }
        size_t backgroundArrayBytes = m_maxStackCount * sizeof(BOOL);
        m_stackInBackground = (BOOL *)malloc(backgroundArrayBytes);
        if (m_stackInBackground != NULL) {
            memset(m_stackInBackground, 0, backgroundArrayBytes);
        }
        m_poolTailPoint = 0;
    }
    return self;
}

- (void)dealloc {
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
    if (m_stackInBackground != NULL) {
        free(m_stackInBackground);
        m_stackInBackground = NULL;
    }
}

- (void)addThreadStack:(uintptr_t *)stackArray andLength:(size_t)stackCount andCPU:(float)stackCPU isInBackground:(BOOL)isInBackground {
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
    m_stackInBackground[m_poolTailPoint] = isInBackground;

    m_poolTailPoint = (m_poolTailPoint + 1) % m_maxStackCount;
}

- (NSArray<NSDictionary *> *)makeCallTree {
    _parentAddressFrame = nil;

    for (int i = 0; i < m_maxStackCount; i++) {
        uintptr_t *curStack = m_stackCyclePool[i];
        size_t curLength = m_stackCount[i];
        WCAddressFrame *curAddressFrame = [self p_getAddressFrameWithStackTraces:curStack length:curLength cpu:m_stackCPU[i] isInBackground:m_stackInBackground[i]];
        [self p_addAddressFrame:curAddressFrame];
    }

    NSMutableArray<NSDictionary *> *addressDictArray = [[NSMutableArray alloc] init];

    if ([self.parentAddressFrame count] > 1) {
        [self.parentAddressFrame sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
            WCAddressFrame *frame1 = (WCAddressFrame *)obj1;
            WCAddressFrame *frame2 = (WCAddressFrame *)obj2;
            if (frame1.repeatCount > frame2.repeatCount) {
                return NSOrderedAscending;
            }
            return NSOrderedDescending;
        }];
    }

    for (int i = 0; i < [self.parentAddressFrame count]; i++) {
        WCAddressFrame *addressFrame = self.parentAddressFrame[i];
        [addressFrame symbolicate];
        NSDictionary *curDict = [self p_getInfoDictFromAddressFrame:addressFrame];
        [addressDictArray addObject:curDict];
    }

    return [addressDictArray copy];
}

- (NSDictionary *)p_getInfoDictFromAddressFrame:(WCAddressFrame *)addressFrame {
    NSMutableDictionary *currentInfoDict = [[addressFrame getInfoDict] mutableCopy];
    NSMutableArray<NSDictionary *> *childInfoDict = [[NSMutableArray alloc] init];

    if ([addressFrame.childAddressFrame count] > 1) {
        [addressFrame.childAddressFrame sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
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

- (WCAddressFrame *)p_getAddressFrameWithStackTraces:(uintptr_t *)stackTrace length:(size_t)traceLength cpu:(float)stackCPU isInBackground:(BOOL)isInBackground {
    if (stackTrace == NULL || traceLength == 0) {
        return nil;
    }
    WCAddressFrame *headAddressFrame = nil;
    WCAddressFrame *currentParentFrame = nil;

    uint32_t repeatWeight = (uint32_t)(stackCPU / 5.);
    for (int i = 0; i < traceLength && i < MAX_STACK_TRACE_IN_LOG; i++) {
        uintptr_t address = stackTrace[i];
        WCAddressFrame *curFrame = [[WCAddressFrame alloc] initWithAddress:address withRepeatCount:repeatWeight isInBackground:isInBackground];
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

- (void)p_addAddressFrame:(WCAddressFrame *)addressFrame {
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

- (void)p_mergeAddressFrame:(WCAddressFrame *)mainFrame with:(WCAddressFrame *)mergedFrame {
    if (mainFrame.address != mergedFrame.address) {
        assert(0);
    }
    mainFrame.repeatCount += mergedFrame.repeatCount;
    mainFrame.repeatCountInBackground += mergedFrame.repeatCountInBackground;

    if (mainFrame.childAddressFrame == nil || [mainFrame.childAddressFrame count] == 0) {
        mainFrame.childAddressFrame = mergedFrame.childAddressFrame;
        return; // full with mergedFrame.childeAddressFrame
    }

    [self p_mergedAddressFrameArray:mainFrame.childAddressFrame with:mergedFrame.childAddressFrame];
}

- (void)p_mergedAddressFrameArray:(NSMutableArray<WCAddressFrame *> *)mainFrameArray with:(NSMutableArray<WCAddressFrame *> *)mergedFrameArray {
    if (mergedFrameArray == nil || [mergedFrameArray count] == 0) {
        return;
    }
    NSMutableArray<WCAddressFrame *> *notFoundFrame = [NSMutableArray array];
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

- (NSString *)description {
    NSMutableString *retStr = [NSMutableString new];

    for (int i = 0; i < [self.parentAddressFrame count]; i++) {
        WCAddressFrame *frame = self.parentAddressFrame[i];
        [retStr appendString:[frame description]];
    }

    return retStr;
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

- (id)initWithThread:(thread_t)cpu_thread andCpuValue:(float)cpu_value {
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
static BOOL g_isInBackground = NO;

@interface WCPowerConsumeStackCollector ()

@property (nonatomic, strong) WCStackTracePool *stackTracePool;

@end

@implementation WCPowerConsumeStackCollector

- (id)initWithCPULimit:(float)cpuLimit {
    self = [super init];
    if (self) {
        g_kGetPowerStackCPULimit = cpuLimit;
        _stackTracePool = [[WCStackTracePool alloc] initWithMaxStackTraceCount:MAX_STACK_TRACE_COUNT];

#if !TARGET_OS_OSX
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(willEnterForeground)
                                                     name:UIApplicationWillEnterForegroundNotification
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(didEnterBackground)
                                                     name:UIApplicationDidEnterBackgroundNotification
                                                   object:nil];
#endif
    }
    return self;
}

- (void)dealloc {
#if !TARGET_OS_OSX
    [[NSNotificationCenter defaultCenter] removeObserver:self];
#endif
}

- (void)willEnterForeground {
    g_isInBackground = NO;
}

- (void)didEnterBackground {
    g_isInBackground = YES;
}

// ============================================================================
#pragma mark - Power Consume Block
// ============================================================================

- (void)makeConclusion {
    WCStackTracePool *handlePool = _stackTracePool;
    _stackTracePool = [[WCStackTracePool alloc] initWithMaxStackTraceCount:MAX_STACK_TRACE_COUNT];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSArray<NSDictionary *> *stackTree = [handlePool makeCallTree];
        if (self.delegate != nil && [self.delegate respondsToSelector:@selector(powerConsumeStackCollectorConclude:)]) {
            [self.delegate powerConsumeStackCollectorConclude:stackTree];
        }
    });
}

- (float)getCPUUsageAndPowerConsumeStack {
    mach_msg_type_number_t thread_count;
    NSMutableArray<WCCpuStackFrame *> *cost_cpu_thread_array = [[NSMutableArray alloc] init];

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
                    [_stackTracePool addThreadStack:stack_matrix[i] andLength:(size_t)trace_length_matrix[i] andCPU:cost_cpu_value_list[i] isInBackground:g_isInBackground];
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

- (BOOL)isCPUHighBlock {
    if (fabs([self getCPUUsageAndCPUBlockStack] + 1) < FLOAT_THRESHOLD) {
        return NO;
    }
    return YES;
}

- (float)getCPUUsageAndCPUBlockStack {
    mach_msg_type_number_t thread_count;
    NSMutableArray<WCCpuStackFrame *> *cost_cpu_thread_array = [[NSMutableArray alloc] init];

    float result = [self getTotCpuWithCostCpuThreadArray:&cost_cpu_thread_array andThreadCount:&thread_count];

    if (fabs(result + 1.0) < FLOAT_THRESHOLD) {
        return -1.0;
    }

    // sort the thread array according to its cost value when cpu high block
    [cost_cpu_thread_array sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
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
            int real_cpu_thread_count = 0;
            for (int i = 0; i < cost_cpu_thread_count; i++) {
                if (stack_matrix[i] != NULL) {
                    real_cpu_thread_count++;
                }
            }

            g_cpuHighThreadArray = (KSStackCursor **)malloc(sizeof(KSStackCursor *) * (int)real_cpu_thread_count);
            g_cpuHighThreadNumber = (int)real_cpu_thread_count;
            g_cpuHighThreadValueArray = (float *)malloc(sizeof(float) * (int)real_cpu_thread_count);

            int index = 0;
            for (int i = 0; i < cost_cpu_thread_count; i++) {
                if (stack_matrix[i] != NULL) {
                    g_cpuHighThreadArray[index] = (KSStackCursor *)malloc(sizeof(KSStackCursor));
                    kssc_initWithBacktrace(g_cpuHighThreadArray[index], stack_matrix[i], trace_length_matrix[i], 0);
                    g_cpuHighThreadValueArray[index] = cost_cpu_value_list[i];
                    index++;
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

- (int)getCurrentCpuHighStackNumber {
    return g_cpuHighThreadNumber;
}

- (KSStackCursor **)getCPUStackCursor {
    return g_cpuHighThreadArray;
}

- (float *)getCpuHighThreadValueArray {
    return g_cpuHighThreadValueArray;
}

// ============================================================================
#pragma mark - Helper Function
// ============================================================================

- (float)getTotCpuWithCostCpuThreadArray:(NSMutableArray<WCCpuStackFrame *> **)cost_cpu_thread_array
                          andThreadCount:(mach_msg_type_number_t *)thread_count {
    // variable declarations
    const task_t thisTask = mach_task_self();
    kern_return_t kr;
    thread_array_t thread_list;

    // get threads in the task
    kr = task_threads(thisTask, &thread_list, thread_count);
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
            tot_cpu = -1;
            goto cleanup;
        }

        thread_basic_info_t basic_info_th = (thread_basic_info_t)thinfo;
        float cur_cpu = 0;

        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            cur_cpu = basic_info_th->cpu_usage / (float)TH_USAGE_SCALE * 100.0;
            tot_cpu = tot_cpu + cur_cpu;
        }

        if (cur_cpu > 5. && current_thread != thisThread && *cost_cpu_thread_array != NULL) {
            WCCpuStackFrame *cpu_stack_frame = [[WCCpuStackFrame alloc] initWithThread:current_thread andCpuValue:cur_cpu];
            [*cost_cpu_thread_array addObject:cpu_stack_frame];
        }
    }

cleanup:
    for (int i = 0; i < *thread_count; i++) {
        mach_port_deallocate(thisTask, thread_list[i]);
    }

    kr = vm_deallocate(thisTask, (vm_offset_t)thread_list, *thread_count * sizeof(thread_t));
    return tot_cpu;
}

- (StackInfo)getStackInfoWithThreadCount:(mach_msg_type_number_t)cost_cpu_thread_count
                       costCpuThreadList:(thread_t *)cost_cpu_thread_list
                        costCpuValueList:(float *)cost_cpu_value_list {
    struct StackInfo result;
    do { // get power-consuming stack
        size_t maxEntries = 100;
        int *trace_length_matrix = (int *)malloc(sizeof(int) * cost_cpu_thread_count);
        if (trace_length_matrix == NULL) {
            break;
        }
        uintptr_t **stack_matrix = (uintptr_t **)malloc(sizeof(uintptr_t *) * cost_cpu_thread_count);
        if (stack_matrix == NULL) {
            free(trace_length_matrix);
            break;
        }
        BOOL have_null = NO;
        for (int i = 0; i < cost_cpu_thread_count; i++) {
            // the alloc size should contain async thread
            stack_matrix[i] = (uintptr_t *)malloc(sizeof(uintptr_t) * maxEntries * 2);
            if (stack_matrix[i] == NULL) {
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
            free(trace_length_matrix);
            break;
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
        }
        ksmc_resumeEnvironment();

        result = { stack_matrix, trace_length_matrix };
    } while (0);
    return result;
}

@end
