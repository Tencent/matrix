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

static float kOverCPULimit = 80.;
static float kOverCPULimitSecLimit = 60.;
static int TICK_TOCK_COUNT = 60;

@interface WCCPUHandler () {
    NSUInteger m_tickTok;

    float m_totalCPUCost;
    float m_totalTrackingTime;
    BOOL m_bTracking;
    
    float m_backgroundTotalCPU;
    float m_backgroundTotalSec;
    BOOL m_background;
    volatile BOOL m_backgroundCPUTooSmall;
}

@end

@implementation WCCPUHandler

- (id)init
{
    return [self initWithCPULimit:80.];
}

- (id)initWithCPULimit:(float)cpuLimit
{
    self = [super init];
    if (self) {
        kOverCPULimit = cpuLimit;
        
        m_tickTok = 0;
        m_bTracking = NO;
        
        m_totalTrackingTime = 0.;
        m_totalCPUCost = 0.;
        
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
#pragma mark - CPU Related
// ============================================================================

- (BOOL)isBackgroundCPUTooSmall
{
    return m_backgroundCPUTooSmall;
}

// run on the check child thread
- (BOOL)cultivateCpuUsage:(float)cpuUsage periodTime:(float)periodSec
{
    [self cultivateBackgroundCpu:cpuUsage periodTime:periodSec];

    if (periodSec < 0 || periodSec > 5.) {
        MatrixDebug(@"abnormal period sec : %f", periodSec);
        return NO;
    }
    
    if (m_tickTok > 0) {
        m_tickTok -= 1; // Annealing algorithm
        if (m_tickTok == 0) {
            MatrixInfo(@"tick tok over");
        }
        return NO;
    }

    if (cpuUsage > kOverCPULimit && m_bTracking == NO) {
        MatrixInfo(@"start track cpu usage");
        m_totalCPUCost = 0.;
        m_totalTrackingTime = 0.;
        m_bTracking = YES;
    }
    
    if (m_bTracking == NO) {
        return NO;
    }
    
    m_totalTrackingTime += periodSec;
    m_totalCPUCost += periodSec * cpuUsage;

    float halfCPUZone = kOverCPULimit * m_totalTrackingTime / 2.;
    
    if (m_totalCPUCost < halfCPUZone) {
        MatrixInfo(@"stop track cpu usage");
        m_totalCPUCost = 0.;
        m_totalTrackingTime = 0.;
        m_bTracking = NO;
        return NO;
    }
    
    if (m_totalTrackingTime >= kOverCPULimitSecLimit) {
        BOOL exceedLimit = NO;
        float fullCPUZone = halfCPUZone + halfCPUZone;
        if (m_totalCPUCost > fullCPUZone) {
            MatrixInfo(@"exceed cpu limit");
            exceedLimit = YES;
        }
        
        if (exceedLimit) {
            m_totalCPUCost = 0;
            m_totalTrackingTime = 0.;
            m_bTracking = NO;
            m_tickTok += TICK_TOCK_COUNT;
        }
        return exceedLimit;
    }
    
    return NO;
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

@end
