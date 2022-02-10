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

#import "WCBlockMonitorConfigHandler.h"
#import <sys/sysctl.h>
#import "MatrixLogDef.h"
#import "MatrixDeviceInfo.h"
#import "WCCrashBlockFileHandler.h"

@interface WCBlockMonitorConfigHandler () {
    WCBlockMonitorConfiguration *m_currentConfiguration;
}

@end

@implementation WCBlockMonitorConfigHandler

- (id)init {
    self = [super init];
    if (self) {
        m_currentConfiguration = [WCBlockMonitorConfiguration defaultConfig];
    }
    return self;
}

- (void)setConfiguration:(WCBlockMonitorConfiguration *)monitorConfig {
    m_currentConfiguration = monitorConfig;
    MatrixInfo(@"%@", [m_currentConfiguration description]);
}

- (useconds_t)getRunloopTimeOut {
    return m_currentConfiguration.runloopTimeOut;
}

- (useconds_t)getRunloopLowThreshold {
    return m_currentConfiguration.runloopLowThreshold;
}

- (BOOL)getRunloopDynamicThresholdEnabled {
    return m_currentConfiguration.bRunloopDynamicThreshold;
}

- (float)getCPUUsagePercent {
    return m_currentConfiguration.limitCPUPercent * [MatrixDeviceInfo cpuCount];
}

- (BOOL)getMainThreadHandle {
    return m_currentConfiguration.bMainThreadHandle;
}

- (useconds_t)getCheckPeriodTime {
    return m_currentConfiguration.checkPeriodTime;
}

- (useconds_t)getPerStackInterval {
    return m_currentConfiguration.perStackInterval;
}

- (BOOL)getMainThreadProfile {
    return m_currentConfiguration.bMainThreadProfile;
}

- (int)getMainThreadCount {
    return m_currentConfiguration.mainThreadCount;
}

+ (int)getDeviceCPUCount {
    size_t size = sizeof(int);
    int results;
    int mib[2] = { CTL_HW, (int)HW_NCPU };
    sysctl(mib, 2, &results, &size, NULL, 0);
    return results;
}

- (BOOL)getShouldPrintCPUUsage {
    return m_currentConfiguration.bPrintCPUUsage;
}

- (BOOL)getShouldGetCPUHighLog {
    return m_currentConfiguration.bGetCPUHighLog;
}

- (BOOL)getShouldGetPowerConsumeStack {
    return m_currentConfiguration.bGetPowerConsumeStack;
}

- (float)getPowerConsumeCPULimit {
    return m_currentConfiguration.powerConsumeStackCPULimit;
}

- (BOOL)getShouldFilterSameStack {
    return m_currentConfiguration.bFilterSameStack;
}

- (uint32_t)getTriggerFilterCount {
    if (m_currentConfiguration.triggerToBeFilteredCount == 0) {
        m_currentConfiguration.triggerToBeFilteredCount = 1;
    }
    return m_currentConfiguration.triggerToBeFilteredCount;
}

- (BOOL)getShouldPrintMemoryUse {
    return m_currentConfiguration.bPrintMemomryUse;
}

- (BOOL)getShouldPrintCPUFrequency {
    return m_currentConfiguration.bPrintCPUFrequency;
}

#pragma mark - Disk IO

- (BOOL)getShouldGetDiskIOStack {
    return m_currentConfiguration.bGetDiskIOStack;
}

- (size_t)getSingleReadLimit {
    return m_currentConfiguration.singleReadLimit;
}

- (size_t)getSingleWriteLimit {
    return m_currentConfiguration.singleWriteLimit;
}

- (size_t)getTotalReadLimit {
    return m_currentConfiguration.totalReadLimit;
}

- (size_t)getTotalWriteLimit {
    return m_currentConfiguration.totalWriteLimit;
}

- (uint32_t)getMemoryWarningThresholdInMB {
    return m_currentConfiguration.memoryWarningThresholdInMB;
}

- (BOOL)getSensitiveRunloopHangDetection {
    return m_currentConfiguration.bSensitiveRunloopHangDetection;
}

- (BOOL)getShouldSuspendAllThreads {
    return m_currentConfiguration.bSuspendAllThreads;
}

- (BOOL)getShouldEnableSnapshot {
    return m_currentConfiguration.bEnableSnapshot;
}

@end
