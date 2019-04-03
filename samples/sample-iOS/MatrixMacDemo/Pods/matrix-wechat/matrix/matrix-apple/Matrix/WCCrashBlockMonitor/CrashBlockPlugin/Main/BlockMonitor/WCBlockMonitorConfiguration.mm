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
#define kCheckPeriodTime "CheckPeriodTime"
#define kMainThreadHandle "MainThreadHandle"
#define kPerStackInterval "PerStackInterval"
#define kMainThreadCount "MainThreadCount"
#define kFrameDropLimit "FrameDropLimit"
#define kGetFPSLog "GetFPSLog"
#define kCPUPercentLimit "CPUPercentLimit"
#define kPrintCPUUsage "PrintCPUUsage"
#define kGetCPUHighLog "GetCPUHigh"
#define kGetCPUIntervalHightLog "GetCPUIntervalHight"
#define kFilterSameStack "FilterSameStack"
#define kTriggerFilteredCount "TriggerFilteredCount"

@implementation WCBlockMonitorConfiguration

+ (id)defaultConfig
{
    WCBlockMonitorConfiguration *configuration = [[WCBlockMonitorConfiguration alloc] init];
    configuration.runloopTimeOut = g_defaultRunLoopTimeOut;
    configuration.checkPeriodTime = g_defaultCheckPeriodTime;
    configuration.bMainThreadHandle = NO;
    configuration.perStackInterval = g_defaultPerStackInterval;
    configuration.mainThreadCount = g_defaultMainThreadCount;
    configuration.limitCPUPercent = g_defaultCPUUsagePercent;
    configuration.bPrintCPUUsage = NO;
    configuration.bGetCPUHighLog = NO;
    configuration.bGetIntervalHighLog = NO;
    configuration.bFilterSameStack = NO;
    configuration.triggerToBeFilteredCount = 10;
    return configuration;
}

- (id)initWithCoder:(NSCoder *)aDecoder
{
    self = [super init];
    if (self) {
        _runloopTimeOut = (uint32_t)[aDecoder decodeInt32ForKey:@kRunloopTimeOut];
        _checkPeriodTime = (uint32_t)[aDecoder decodeInt32ForKey:@kCheckPeriodTime];
        _bMainThreadHandle = [aDecoder decodeBoolForKey:@kMainThreadHandle];
        _perStackInterval = (uint32_t)[aDecoder decodeInt64ForKey:@kPerStackInterval];
        _mainThreadCount = (uint32_t)[aDecoder decodeInt32ForKey:@kMainThreadCount];
        _limitCPUPercent = [aDecoder decodeFloatForKey:@kCPUPercentLimit];
        _bPrintCPUUsage = [aDecoder decodeBoolForKey:@kPrintCPUUsage];
        _bGetCPUHighLog = [aDecoder decodeBoolForKey:@kGetCPUHighLog];
        _bGetIntervalHighLog = [aDecoder decodeBoolForKey:@kGetCPUIntervalHightLog];
        _bFilterSameStack = [aDecoder decodeBoolForKey:@kFilterSameStack];
        _triggerToBeFilteredCount = (uint32_t)[aDecoder decodeInt32ForKey:@kTriggerFilteredCount];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
    [aCoder encodeInt64:_runloopTimeOut forKey:@kRunloopTimeOut];
    [aCoder encodeInt32:_checkPeriodTime forKey:@kCheckPeriodTime];
    [aCoder encodeBool:_bMainThreadHandle forKey:@kMainThreadHandle];
    [aCoder encodeInt32:_perStackInterval forKey:@kPerStackInterval];
    [aCoder encodeInt32:_mainThreadCount forKey:@kMainThreadCount];
    [aCoder encodeFloat:_limitCPUPercent forKey:@kCPUPercentLimit];
    [aCoder encodeBool:_bPrintCPUUsage forKey:@kPrintCPUUsage];
    [aCoder encodeBool:_bGetCPUHighLog forKey:@kGetCPUHighLog];
    [aCoder encodeBool:_bGetIntervalHighLog forKey:@kGetCPUIntervalHightLog];
    [aCoder encodeBool:_bFilterSameStack forKey:@kFilterSameStack];
    [aCoder encodeInt32:_triggerToBeFilteredCount forKey:@kTriggerFilteredCount];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"RunLoopTimeOut[%u],CheckPeriodTime[%d],\
            \nMainThreadHandle[%d],PerStackInterval[%d],MainThreadCount[%d],\
            \nCPUUsagePercent[%f],PrintCPU[%d],GetCPUHighLog[%d],GetCPUIntervalHighLog[%d],\
            \nFilterSameStact[%d],TriggerBeFilteredCount[%u]",
            self.checkPeriodTime, self.runloopTimeOut,
            self.bMainThreadHandle, self.perStackInterval, self.mainThreadCount,
            self.limitCPUPercent, self.bPrintCPUUsage, self.bGetCPUHighLog, self.bGetIntervalHighLog,
            self.bFilterSameStack, self.triggerToBeFilteredCount];
}

@end

