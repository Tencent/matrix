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

#import "WCBlockMonitorConfiguration.h"

@implementation WCBlockMonitorConfiguration

+ (id)defaultConfig {
    WCBlockMonitorConfiguration *configuration = [[WCBlockMonitorConfiguration alloc] init];
    configuration.runloopTimeOut = g_defaultRunLoopTimeOut;
    configuration.runloopLowThreshold = g_defaultRunLoopTimeOut;
    configuration.bRunloopDynamicThreshold = NO;
    configuration.checkPeriodTime = g_defaultCheckPeriodTime;
    configuration.bMainThreadHandle = NO;
    configuration.perStackInterval = g_defaultPerStackInterval;
    configuration.mainThreadCount = g_defaultMainThreadCount;
    configuration.bMainThreadProfile = NO;
    configuration.limitCPUPercent = g_defaultCPUUsagePercent;
    configuration.bPrintCPUUsage = NO;
    configuration.bGetCPUHighLog = NO;

    configuration.bGetPowerConsumeStack = NO;
    configuration.powerConsumeStackCPULimit = g_defaultPowerConsumeCPULimit;
    configuration.bFilterSameStack = NO;
    configuration.triggerToBeFilteredCount = 10;
    configuration.dumpDailyLimit = g_defaultDumpDailyLimit;
    configuration.bPrintMemomryUse = NO;
    configuration.bPrintCPUFrequency = NO;
    configuration.bGetDiskIOStack = NO;
    configuration.singleReadLimit = g_defaultSingleReadLimit;
    configuration.singleWriteLimit = g_defaultSingleWriteLimit;
    configuration.totalReadLimit = g_defaultTotalReadLimit;
    configuration.totalWriteLimit = g_defaultTotalWriteLimit;
    configuration.memoryWarningThresholdInMB = g_defaultMemoryThresholdInMB;
    configuration.bSensitiveRunloopHangDetection = NO;
    configuration.bSuspendAllThreads = YES;
    configuration.bEnableSnapshot = YES;

    return configuration;
}

- (NSString *)description {
    NSDictionary *properties = @{
        @"runloopTimeOut": @(self.runloopTimeOut),
        @"runloopLowThreshold": @(self.runloopLowThreshold),
        @"bRunloopDynamicThreshold": @(self.bRunloopDynamicThreshold),
        @"bMainThreadHandle": @(self.bMainThreadHandle),
        @"perStackInterval": @(self.perStackInterval),
        @"bMainThreadProfile": @(self.bMainThreadProfile),
        @"limitCPUPercent": @(self.limitCPUPercent),
        @"bPrintCPUUsage": @(self.bPrintCPUUsage),
        @"bGetCPUHighLog": @(self.bGetCPUHighLog),
        @"bGetPowerConsumeStack": @(self.bGetPowerConsumeStack),
        @"powerConsumeStackCPULimit": @(self.powerConsumeStackCPULimit),
        @"dumpDailyLimit": @(self.dumpDailyLimit),
        @"bPrintMemomryUse": @(self.bPrintMemomryUse),
        @"bPrintCPUFrequency": @(self.bPrintCPUFrequency),
        @"memoryWarningThresholdInMB": @(self.memoryWarningThresholdInMB),
        @"bSensitiveRunloopHangDetection": @(self.bSensitiveRunloopHangDetection),
        @"bSuspendAllThreads": @(self.bSuspendAllThreads),
        @"bEnableSnapshot": @(self.bEnableSnapshot),
    };
    return [properties description];
}

@end
