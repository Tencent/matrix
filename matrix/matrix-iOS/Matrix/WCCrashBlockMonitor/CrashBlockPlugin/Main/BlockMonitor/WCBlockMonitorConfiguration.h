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

#import <Foundation/Foundation.h>

#define BM_MicroFormat_MillSecond 1000
#define BM_MicroFormat_Second 1000000
#define BM_MicroFormat_FrameMillSecond 16000

const static useconds_t g_defaultRunLoopTimeOut = 2 * BM_MicroFormat_Second;
const static useconds_t g_defaultCheckPeriodTime = 1 * BM_MicroFormat_Second;
const static useconds_t g_defaultPerStackInterval = 50 * BM_MicroFormat_MillSecond;
const static float g_defaultCPUUsagePercent = 80.;
const static float g_defaultPowerConsumeCPULimit = 80.;
const static int g_defaultMainThreadCount = 10;
const static int g_defaultFrameDropCount = 8;

@interface WCBlockMonitorConfiguration : NSObject <NSCoding>

+ (id)defaultConfig;

/// define the timeout of the main runloop
@property (nonatomic, assign) useconds_t runloopTimeOut;

/// define the checking period of the main runloop
@property (nonatomic, assign) useconds_t checkPeriodTime;

/// enable the main thread handle, hhether to handle the main thread to get the most time-consuming stack recently
@property (nonatomic, assign) BOOL bMainThreadHandle;

/// define the interval of the acquire of the main thread stack
@property (nonatomic, assign) useconds_t perStackInterval;

/// define the count of the main thread stack that be saved
@property (nonatomic, assign) uint32_t mainThreadCount;

/// define the limit of singe core's comsuption
@property (nonatomic, assign) float limitCPUPercent;

/// enable printing the cpu usage in the log
@property (nonatomic, assign) BOOL bPrintCPUUsage;

/// enable get the log of CPU that CPU too high ( current CPU usage > limitCPUPercent * cpu_count )
@property (nonatomic, assign) BOOL bGetCPUHighLog;

/// enable get the "cost cpu" callstack
@property (nonatomic, assign) BOOL bGetPowerConsumeStack;

/// define the value of CPU considered to be power consuming
@property (nonatomic, assign) float powerConsumeStackCPULimit;

/// enable to filter the same stack in one day, the stack be captured over "triggerToBeFilteredCount" times would be filtered
@property (nonatomic, assign) BOOL bFilterSameStack;

/// define the count that a stack can be captured in one day, see above "bFilterSameStack"
@property (nonatomic, assign) uint32_t triggerToBeFilteredCount;

/// enable printing the memory use
@property (nonatomic, assign) BOOL bPrintMemomryUse;

@end
