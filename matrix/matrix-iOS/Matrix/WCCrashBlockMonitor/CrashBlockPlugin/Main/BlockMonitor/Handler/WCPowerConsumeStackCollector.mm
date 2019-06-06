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

@end

// ============================================================================
#pragma mark - WCStackTracePool
// ============================================================================


@interface WCStackTracePool ()
{
    uintptr_t **m_stackCyclePool;
    size_t *m_stackCount;
    uint64_t m_poolTailPoint;
    size_t m_maxStackCount;
}

/* conlusion */
@property (nonatomic, strong) NSMutableArray <WCAddressFrame *>*parentAddressFrame;
@property (nonatomic, assign) NSUInteger limitRepeatCount;

@end

@implementation WCStackTracePool

- (id)init
{
    return [self initWithMaxStackTraceCount:10 withLimitRepeat:10];
}

- (id)initWithMaxStackTraceCount:(NSUInteger)maxStackTraceCount withLimitRepeat:(NSUInteger)limitRepeatCount
{
    self = [super init];
    if (self) {
        _limitRepeatCount = limitRepeatCount;
        
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
}

- (void)addThreadStack:(uintptr_t *)stackArray andLength:(size_t)stackCount
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
    
    m_poolTailPoint = (m_poolTailPoint + 1) % m_maxStackCount;
}

- (NSArray <NSDictionary *>*)makeCallTree
{
    _parentAddressFrame = nil;
    
    for (int i = 0; i < m_maxStackCount; i++) {
        uintptr_t *curStack = m_stackCyclePool[i];
        size_t curLength = m_stackCount[i];
        WCAddressFrame *curAddressFrame = [self p_getAddressFrameWithStackTraces:curStack length:curLength];
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
    
    for (WCAddressFrame *addressFrame in self.parentAddressFrame) {
        if (addressFrame.repeatCount >= _limitRepeatCount) {
            [addressFrame symbolicate];
            NSDictionary *curDict = [self p_getInfoDictFromAddressFrame:addressFrame];
            [addressDictArray addObject:curDict];
        }
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

- (WCAddressFrame *)p_getAddressFrameWithStackTraces:(uintptr_t *)stackTrace length:(size_t)traceLength
{
    if (stackTrace == NULL || traceLength== 0) {
        return nil;
    }
    WCAddressFrame *headAddressFrame = nil;
    WCAddressFrame *currentParentFrame = nil;
    for (int i = 0; i < traceLength; i++) {
        uintptr_t address = stackTrace[i];
        WCAddressFrame *curFrame = [[WCAddressFrame alloc] initWithAddress:address withRepeatCount:1];
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
            if (tmpFrame.address == addressFrame.address) {
                foundAddressFrame = tmpFrame;
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
    mainFrame.repeatCount += 1;
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
#pragma mark - WCPowerConsumeStackCollector
// ============================================================================

static float kGetPowerStackCPULimit = 80.;

@interface WCPowerConsumeStackCollector ()

@property (nonatomic, strong) WCStackTracePool *stackTracePool;

@end

@implementation WCPowerConsumeStackCollector

- (id)initWithCPULimit:(float)cpuLimit
{
    self = [super init];
    if (self) {
        kGetPowerStackCPULimit = cpuLimit;
        _stackTracePool = [[WCStackTracePool alloc] initWithMaxStackTraceCount:100 withLimitRepeat:10];
    }
    return self;
}

- (void)makeConclusion
{
    WCStackTracePool *handlePool = _stackTracePool;
    _stackTracePool = [[WCStackTracePool alloc] initWithMaxStackTraceCount:100 withLimitRepeat:10];
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSArray <NSDictionary *>*stackTree = [handlePool makeCallTree];
        if (self.delegate != nil && [self.delegate respondsToSelector:@selector(powerConsumeStackCollectorConclude:)]) {
            [self.delegate powerConsumeStackCollectorConclude:stackTree];
        }
    });
}

- (float)getCPUUsageAndPowerConsumeStack
{
    kern_return_t kr;
    task_info_data_t tinfo;
    mach_msg_type_number_t task_info_count;
    
    task_info_count = TASK_INFO_MAX;
    kr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)tinfo, &task_info_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    thread_array_t thread_list;
    mach_msg_type_number_t thread_count;
    
    // get threads in the task
    kr = task_threads(mach_task_self(), &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    long tot_sec = 0;
    long tot_usec = 0;
    float tot_cpu = 0;
    
    thread_t *cost_cpu_thread_list = (thread_t *)malloc(sizeof(thread_t) * thread_count);
    mach_msg_type_number_t cost_cpu_thread_count = 0;
    int cost_cpu_j = 0;
    
    for (int j = 0; j < thread_count; j++) {
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
            tot_sec = tot_sec + cur_sec;
            cur_usec = basic_info_th->system_time.microseconds + basic_info_th->system_time.microseconds;
            tot_usec = tot_usec + cur_usec;
            cur_cpu = basic_info_th->cpu_usage / (float) TH_USAGE_SCALE * 100.0;
            tot_cpu = tot_cpu + cur_cpu;
        }
        
        if (cur_cpu > 5.) {
            cost_cpu_thread_list[cost_cpu_j] = current_thread;
            cost_cpu_j ++;
            cost_cpu_thread_count ++;
        }
    }
    
    if (tot_cpu > kGetPowerStackCPULimit && cost_cpu_thread_count > 0) {
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
                stack_matrix[i] = (uintptr_t *)malloc(sizeof(uintptr_t) * maxEntries);
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
            
            ksmc_suspendEnvironment();
            for (int i = 0; i < cost_cpu_thread_count; i++) {
                thread_t current_thread = cost_cpu_thread_list[i];
                uintptr_t backtrace_buffer[maxEntries];
                trace_length_matrix[i] = kssc_backtraceCurrentThread(current_thread, backtrace_buffer, (int)maxEntries);
                for (int j = 0; j < trace_length_matrix[i]; j++) {
                    stack_matrix[i][j] = backtrace_buffer[j];
                }
            }
            ksmc_resumeEnvironment();

            for (int i = 0; i < cost_cpu_thread_count; i++) {
                [_stackTracePool addThreadStack:stack_matrix[i] andLength:(size_t)trace_length_matrix[i]];
            }
            free(trace_length_matrix);
        } while (0);
    }
    
    kr = vm_deallocate(mach_task_self(), (vm_offset_t) thread_list, thread_count * sizeof(thread_t));
    free(cost_cpu_thread_list);
    return tot_cpu;
}

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
    
    long tot_sec = 0;
    long tot_usec = 0;
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
            tot_sec = tot_sec + basic_info_th->user_time.seconds + basic_info_th->system_time.seconds;
            tot_usec = tot_usec + basic_info_th->system_time.microseconds + basic_info_th->system_time.microseconds;
            tot_cpu = tot_cpu + basic_info_th->cpu_usage / (float) TH_USAGE_SCALE * 100.0;
        }
    }
    
    kr = vm_deallocate(mach_task_self(), (vm_offset_t) thread_list, thread_count * sizeof(thread_t));
    return tot_cpu;
}

@end
