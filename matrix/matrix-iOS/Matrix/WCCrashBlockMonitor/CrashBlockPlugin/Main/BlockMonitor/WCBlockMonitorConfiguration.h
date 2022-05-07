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
const static size_t g_defaultSingleReadLimit = 100 * 1024;
const static size_t g_defaultSingleWriteLimit = 100 * 1024;
const static size_t g_defaultTotalReadLimit = 500 * 1024 * 1024;
const static size_t g_defaultTotalWriteLimit = 200 * 1024 * 1024;
const static uint32_t g_defaultMemoryThresholdInMB = 1024;
const static int g_defaultDumpDailyLimit = 100;

@interface WCBlockMonitorConfiguration : NSObject

+ (id)defaultConfig;

/// define the timeout of the main runloop
@property (nonatomic, assign) useconds_t runloopTimeOut;

/// define the suggested value when lowering the runloop threshold
@property (nonatomic, assign) useconds_t runloopLowThreshold;

/// enable runloop threshold dynamic adjustment
@property (nonatomic, assign) BOOL bRunloopDynamicThreshold;

/// define the checking period of the main runloop
@property (nonatomic, assign) useconds_t checkPeriodTime DEPRECATED_MSG_ATTRIBUTE("depends on runloopTimeOut");

/// enable the main thread handle, whether to handle the main thread to get the most time-consuming stack recently
@property (nonatomic, assign) BOOL bMainThreadHandle;

/// enable the main thread profile, whether to handle the main thread to get all stacks after merging
@property (nonatomic, assign) BOOL bMainThreadProfile;

/// define the interval of the acquire of the main thread stack
@property (nonatomic, assign) useconds_t perStackInterval;

/// define the count of the main thread stack that be saved
@property (nonatomic, assign) uint32_t mainThreadCount DEPRECATED_MSG_ATTRIBUTE("depends on runloopTimeOut");

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
@property (nonatomic, assign) BOOL bFilterSameStack DEPRECATED_MSG_ATTRIBUTE("use dumpDailyLimit instead");

/// define the count that a stack can be captured in one day, see above "bFilterSameStack"
@property (nonatomic, assign) uint32_t triggerToBeFilteredCount DEPRECATED_MSG_ATTRIBUTE("use dumpDailyLimit instead");

/// define the max number of lag dump per day
@property (nonatomic, assign) uint32_t dumpDailyLimit;

/// enable printing the memory use
@property (nonatomic, assign) BOOL bPrintMemomryUse;

/// enable printing the cpu frequency
@property (nonatomic, assign) BOOL bPrintCPUFrequency;

/// enable get the "disk io" callstack
@property (nonatomic, assign) BOOL bGetDiskIOStack DEPRECATED_MSG_ATTRIBUTE("feature removed");

/// define the value of single fd read limit
@property (nonatomic, assign) size_t singleReadLimit DEPRECATED_MSG_ATTRIBUTE("feature removed");

/// define the value of single fd write limit
@property (nonatomic, assign) size_t singleWriteLimit DEPRECATED_MSG_ATTRIBUTE("feature removed");

/// define the value of total read limit in 1s
@property (nonatomic, assign) size_t totalReadLimit DEPRECATED_MSG_ATTRIBUTE("feature removed");

/// define the value of total write limit in 1s
@property (nonatomic, assign) size_t totalWriteLimit DEPRECATED_MSG_ATTRIBUTE("feature removed");

/// define the threshold of memory warning
@property (nonatomic, assign) uint32_t memoryWarningThresholdInMB;

/// enable to detect any runloop hangs on main thread
@property (nonatomic, assign) BOOL bSensitiveRunloopHangDetection;

/// define whether to suspend all threads when handling user exception
@property (nonatomic, assign) BOOL bSuspendAllThreads;

/// define whether to enable snapshot when handling user exception
@property (nonatomic, assign) BOOL bEnableSnapshot;

@end
