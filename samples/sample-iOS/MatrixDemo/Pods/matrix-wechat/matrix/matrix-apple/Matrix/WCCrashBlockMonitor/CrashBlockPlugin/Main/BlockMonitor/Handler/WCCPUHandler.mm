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

#import "WCCPUHandler.h"
#import <TargetConditionals.h>

#import <dlfcn.h>
#import <mach/port.h>
#import <mach/kern_return.h>
#import <mach/mach.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif

#import "MatrixLogDef.h"

#define kForegroundOverEightySecLimit 60  // foreground CPU over 80%.
#define kBakcgroundOverEightySecLimit 60  // background CPU over 80%.
#define TICK_TOCK_COUNT 60

@interface WCCPUHandler () {
    NSUInteger m_tickTok;
    
    BOOL m_bLastOverEighty;
    float m_foregroundOverEightyTotalSec;
    float m_backgroundOverEightyTotalSec;
    float m_backgroundTotalCPU;
    float m_backgroundTotalSec;
    BOOL m_background;
    volatile BOOL m_backgroundCPUTooSmall;
}

@end

@implementation WCCPUHandler

- (id)init
{
    self = [super init];
    if (self) {
        m_tickTok = 0;
        m_bLastOverEighty = NO;
        m_foregroundOverEightyTotalSec = 0;
        m_backgroundOverEightyTotalSec = 0;
        
        m_background = NO;
        m_backgroundTotalCPU = 0.;
        m_backgroundTotalSec = 0.;
        m_backgroundCPUTooSmall = NO;
        
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

- (void)willEnterForeground
{
    m_background = NO;
    m_backgroundCPUTooSmall = NO;
    m_backgroundTotalCPU = 0.;
    m_backgroundTotalSec = 0.;
}

- (void)didEnterBackground
{
    m_background = YES;
    m_backgroundCPUTooSmall = NO;
    m_backgroundTotalCPU = 0.;
    m_backgroundTotalSec = 0.;
}

// ============================================================================
#pragma mark - CPU Related -
// ============================================================================

- (BOOL)isBackgroundCPUTooSmall
{
    return m_backgroundCPUTooSmall;
}

- (BOOL)cultivateCpuUsage:(float)cpuUsage periodTime:(float)periodSec
{
    [self cultivateBackgroundCpu:cpuUsage periodTime:periodSec];

    if (m_tickTok > 0) {
        m_tickTok -= 1; // Annealing algorithm
        if (m_tickTok == 0) {
            MatrixInfo(@"tick tok over");
        }
    }

    if (cpuUsage > 80. && m_tickTok == 0 && m_bLastOverEighty == NO) {
        MatrixInfo(@"start track cpu usage");
        m_foregroundOverEightyTotalSec = 0;
        m_backgroundOverEightyTotalSec = 0;
        m_bLastOverEighty = YES;
    }
    if (cpuUsage <= 80. && m_bLastOverEighty == YES) {
        MatrixInfo(@"stop track cpu usage");
        m_foregroundOverEightyTotalSec = 0;
        m_backgroundOverEightyTotalSec = 0;
        m_bLastOverEighty = NO;
    }
    
    BOOL exceedLimit = NO;
    if (m_bLastOverEighty && m_tickTok == 0) {
        if (m_background) {
            m_foregroundOverEightyTotalSec = 0;
            m_backgroundOverEightyTotalSec += periodSec;
            if (m_backgroundOverEightyTotalSec > kBakcgroundOverEightySecLimit) {
                MatrixInfo(@"background, exceed cpu limit");
                exceedLimit = YES;
            }
        } else {
            m_backgroundOverEightyTotalSec = 0;
            m_foregroundOverEightyTotalSec += periodSec;
            if (m_foregroundOverEightyTotalSec > kForegroundOverEightySecLimit) {
                MatrixInfo(@"foreground, exceed cpu limit");
                exceedLimit = YES;
            }
        }
    }
    if (exceedLimit) {
        m_foregroundOverEightyTotalSec = 0;
        m_backgroundOverEightyTotalSec = 0;
        m_tickTok += TICK_TOCK_COUNT;
    }

    return exceedLimit;
}

- (void)cultivateBackgroundCpu:(float)cpuUsage periodTime:(float)periodSec
{
    if (m_background == NO) {
        return;
    }
    if (m_backgroundTotalSec > 4.9) {
        float cpuPerSec = m_backgroundTotalCPU / m_backgroundTotalSec;
        MatrixDebug(@"background, cpu per sec: %f", cpuPerSec);
        if (cpuPerSec < 6.) {
            m_backgroundCPUTooSmall = YES;
            m_background = NO;
            MatrixInfo(@"background, cpu per sec: %f, too small, m_backgroundCPUTooSmall: %d", cpuPerSec, m_backgroundCPUTooSmall);
        }
        m_backgroundTotalCPU = 0.;
        m_backgroundTotalSec = 0.;
    }
    m_backgroundTotalCPU += cpuUsage * periodSec;
    m_backgroundTotalSec += periodSec;
}


+ (float)getCurrentCpuUsage
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

    thread_info_data_t thinfo;
    mach_msg_type_number_t thread_info_count;

    thread_basic_info_t basic_info_th;

    // get threads in the task
    kr = task_threads(mach_task_self(), &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }

    long tot_sec = 0;
    long tot_usec = 0;
    float tot_cpu = 0;
    int j;

    for (j = 0; j < thread_count; j++) {
        thread_info_count = THREAD_INFO_MAX;
        kr = thread_info(thread_list[j], THREAD_BASIC_INFO,
                         (thread_info_t) thinfo, &thread_info_count);
        if (kr != KERN_SUCCESS) {
            return -1;
        }

        basic_info_th = (thread_basic_info_t) thinfo;

        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            tot_sec = tot_sec + basic_info_th->user_time.seconds + basic_info_th->system_time.seconds;
            tot_usec = tot_usec + basic_info_th->system_time.microseconds + basic_info_th->system_time.microseconds;
            tot_cpu = tot_cpu + basic_info_th->cpu_usage / (float) TH_USAGE_SCALE * 100.0;
        }

    } // for each thread

    kr = vm_deallocate(mach_task_self(), (vm_offset_t) thread_list, thread_count * sizeof(thread_t));
    assert(kr == KERN_SUCCESS);

    return tot_cpu;
}

@end
