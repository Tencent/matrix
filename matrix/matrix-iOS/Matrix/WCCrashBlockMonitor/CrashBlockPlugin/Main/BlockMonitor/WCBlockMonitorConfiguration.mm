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

#define kMonitor "Monitor"
#define kRunloopTimeOut "RunloopTimeOut"
#define kRunloopLowThreshold "RunloopLowThreshold"
#define kRunloopDynamicThresholdEnabled "RunloopDynamicThresholdEnabled"
#define kCheckPeriodTime "CheckPeriodTime"
#define kMainThreadHandle "MainThreadHandle"
#define kMainThreadProfile "MainThreadProfile"
#define kPerStackInterval "PerStackInterval"
#define kMainThreadCount "MainThreadCount"
#define kFrameDropLimit "FrameDropLimit"
#define kGetFPSLog "GetFPSLog"
#define kCPUPercentLimit "CPUPercentLimit"
#define kPrintCPUUsage "PrintCPUUsage"
#define kGetCPUHighLog "GetCPUHigh"
#define kGetPowerConsumeStack "GetPowerConsumeStack"
#define kPowerConsumeCPULimit "PowerConsumeCPULimit"
#define kFilterSameStack "FilterSameStack"
#define kTriggerFilteredCount "TriggerFilteredCount"
#define kPrintMemoryUsage "PrintMemoryUse"
#define kGetDiskIOStack "GetDiskIOStack"
#define kSingleReadLimit "SingleReadLimit"
#define kSingleWriteLimit "SingleWriteLimit"
#define kTotalReadLimit "TotalReadLimit"
#define kTotalWriteLimit "TotalWriteLimit"
#define kPrintCPUFrequency "PrintCPUFrequency"
#define kMemoryWarningThresholdInMB "MemoryWarningThresholdInMB"
#define kSensitiveRunloopHangDetection "SensitiveRunloopHangDetection"
#define kSuspendAllThreads "SuspendAllThreads"
#define kEnableSnapshot "EnableSnapshot"

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

- (id)initWithCoder:(NSCoder *)aDecoder {
    self = [super init];
    if (self) {
        _runloopTimeOut = (useconds_t)[aDecoder decodeInt32ForKey:@kRunloopTimeOut];
        _runloopLowThreshold = (useconds_t)[aDecoder decodeInt32ForKey:@kRunloopLowThreshold];
        _bRunloopDynamicThreshold = [aDecoder decodeBoolForKey:@kRunloopDynamicThresholdEnabled];
        _checkPeriodTime = (useconds_t)[aDecoder decodeInt32ForKey:@kCheckPeriodTime];
        _bMainThreadHandle = [aDecoder decodeBoolForKey:@kMainThreadHandle];
        _perStackInterval = (useconds_t)[aDecoder decodeInt32ForKey:@kPerStackInterval];
        _mainThreadCount = (uint32_t)[aDecoder decodeInt32ForKey:@kMainThreadCount];
        _bMainThreadProfile = [aDecoder decodeBoolForKey:@kMainThreadProfile];
        _limitCPUPercent = [aDecoder decodeFloatForKey:@kCPUPercentLimit];
        _bPrintCPUUsage = [aDecoder decodeBoolForKey:@kPrintCPUUsage];
        _bGetCPUHighLog = [aDecoder decodeBoolForKey:@kGetCPUHighLog];
        _bGetPowerConsumeStack = [aDecoder decodeBoolForKey:@kGetPowerConsumeStack];
        _powerConsumeStackCPULimit = [aDecoder decodeFloatForKey:@kPowerConsumeCPULimit];
        _bFilterSameStack = [aDecoder decodeBoolForKey:@kFilterSameStack];
        _triggerToBeFilteredCount = (uint32_t)[aDecoder decodeInt32ForKey:@kTriggerFilteredCount];
        _bPrintMemomryUse = [aDecoder decodeBoolForKey:@kPrintMemoryUsage];
        _bGetDiskIOStack = [aDecoder decodeBoolForKey:@kGetDiskIOStack];
        _singleReadLimit = [aDecoder decodeInt64ForKey:@kSingleReadLimit];
        _singleWriteLimit = [aDecoder decodeInt64ForKey:@kSingleWriteLimit];
        _totalReadLimit = [aDecoder decodeInt64ForKey:@kTotalReadLimit];
        _totalWriteLimit = [aDecoder decodeInt64ForKey:@kTotalWriteLimit];
        _bPrintCPUFrequency = [aDecoder decodeBoolForKey:@kPrintCPUFrequency];
        _memoryWarningThresholdInMB = (uint32_t)[aDecoder decodeInt32ForKey:@kMemoryWarningThresholdInMB];
        _bSensitiveRunloopHangDetection = [aDecoder decodeBoolForKey:@kSensitiveRunloopHangDetection];
        _bSuspendAllThreads = [aDecoder decodeBoolForKey:@kSuspendAllThreads];
        _bEnableSnapshot = [aDecoder decodeBoolForKey:@kEnableSnapshot];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)aCoder {
    [aCoder encodeInt32:_runloopTimeOut forKey:@kRunloopTimeOut];
    [aCoder encodeInt32:_runloopLowThreshold forKey:@kRunloopLowThreshold];
    [aCoder encodeBool:_bRunloopDynamicThreshold forKey:@kRunloopDynamicThresholdEnabled];
    [aCoder encodeInt32:_checkPeriodTime forKey:@kCheckPeriodTime];
    [aCoder encodeBool:_bMainThreadHandle forKey:@kMainThreadHandle];
    [aCoder encodeInt32:_perStackInterval forKey:@kPerStackInterval];
    [aCoder encodeInt32:_mainThreadCount forKey:@kMainThreadCount];
    [aCoder encodeBool:_bMainThreadProfile forKey:@kMainThreadProfile];
    [aCoder encodeFloat:_limitCPUPercent forKey:@kCPUPercentLimit];
    [aCoder encodeBool:_bPrintCPUUsage forKey:@kPrintCPUUsage];
    [aCoder encodeBool:_bGetCPUHighLog forKey:@kGetCPUHighLog];
    [aCoder encodeBool:_bGetPowerConsumeStack forKey:@kGetPowerConsumeStack];
    [aCoder encodeFloat:_powerConsumeStackCPULimit forKey:@kPowerConsumeCPULimit];
    [aCoder encodeBool:_bFilterSameStack forKey:@kFilterSameStack];
    [aCoder encodeInt32:_triggerToBeFilteredCount forKey:@kTriggerFilteredCount];
    [aCoder encodeBool:_bPrintMemomryUse forKey:@kPrintMemoryUsage];
    [aCoder encodeBool:_bGetDiskIOStack forKey:@kGetDiskIOStack];
    [aCoder encodeInt64:_singleReadLimit forKey:@kSingleReadLimit];
    [aCoder encodeInt64:_singleWriteLimit forKey:@kSingleWriteLimit];
    [aCoder encodeInt64:_totalReadLimit forKey:@kTotalReadLimit];
    [aCoder encodeInt64:_totalWriteLimit forKey:@kTotalWriteLimit];
    [aCoder encodeBool:_bPrintCPUFrequency forKey:@kPrintCPUFrequency];
    [aCoder encodeInt32:_memoryWarningThresholdInMB forKey:@kMemoryWarningThresholdInMB];
    [aCoder encodeBool:_bSensitiveRunloopHangDetection forKey:@kSensitiveRunloopHangDetection];
    [aCoder encodeBool:_bSuspendAllThreads forKey:@kSuspendAllThreads];
    [aCoder encodeBool:_bEnableSnapshot forKey:@kEnableSnapshot];
}

- (NSString *)description {
    NSDictionary *properties = @{
        @"runloopTimeOut": @(self.runloopTimeOut),
        @"runloopLowThreshold": @(self.runloopLowThreshold),
        @"bRunloopDynamicThreshold": @(self.bRunloopDynamicThreshold),
        @"checkPeriodTime": @(self.checkPeriodTime),
        @"bMainThreadHandle": @(self.bMainThreadHandle),
        @"perStackInterval": @(self.perStackInterval),
        @"mainThreadCount": @(self.mainThreadCount),
        @"bMainThreadProfile": @(self.bMainThreadProfile),
        @"limitCPUPercent": @(self.limitCPUPercent),
        @"bPrintCPUUsage": @(self.bPrintCPUUsage),
        @"bGetCPUHighLog": @(self.bGetCPUHighLog),
        @"bGetPowerConsumeStack": @(self.bGetPowerConsumeStack),
        @"powerConsumeStackCPULimit": @(self.powerConsumeStackCPULimit),
        @"bFilterSameStack": @(self.bFilterSameStack),
        @"triggerToBeFilteredCount": @(self.triggerToBeFilteredCount),
        @"bPrintMemomryUse": @(self.bPrintMemomryUse),
        @"bPrintCPUFrequency": @(self.bPrintCPUFrequency),
        @"bGetDiskIOStack": @(self.bGetDiskIOStack),
        @"singleReadLimit": @(self.singleReadLimit),
        @"singleWriteLimit": @(self.singleWriteLimit),
        @"totalReadLimit": @(self.totalReadLimit),
        @"totalWriteLimit": @(self.totalWriteLimit),
        @"memoryWarningThresholdInMB": @(self.memoryWarningThresholdInMB),
        @"bSensitiveRunloopHangDetection": @(self.bSensitiveRunloopHangDetection),
        @"bSuspendAllThreads": @(self.bSuspendAllThreads),
        @"bEnableSnapshot": @(self.bEnableSnapshot),
    };
    return [properties description];
}

@end
