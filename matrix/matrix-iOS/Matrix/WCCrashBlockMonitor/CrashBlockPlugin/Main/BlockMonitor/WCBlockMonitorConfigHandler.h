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
#import "WCBlockMonitorConfiguration.h"

@interface WCBlockMonitorConfigHandler : NSObject

- (void)setConfiguration:(WCBlockMonitorConfiguration *)monitorConfig;

- (useconds_t)getRunloopTimeOut;
- (useconds_t)getRunloopLowThreshold;
- (BOOL)getRunloopDynamicThresholdEnabled;
- (useconds_t)getCheckPeriodTime DEPRECATED_MSG_ATTRIBUTE("depends on runloopTimeOut");
- (BOOL)getMainThreadHandle;
- (useconds_t)getPerStackInterval;
- (int)getMainThreadCount DEPRECATED_MSG_ATTRIBUTE("depends on runloopTimeOut");
- (BOOL)getMainThreadProfile;
- (float)getCPUUsagePercent;
- (BOOL)getShouldPrintCPUUsage;
- (BOOL)getShouldGetCPUHighLog;
- (BOOL)getShouldGetPowerConsumeStack;
- (float)getPowerConsumeCPULimit;
- (BOOL)getShouldFilterSameStack DEPRECATED_MSG_ATTRIBUTE("use dumpDailyLimit instead");
- (uint32_t)getTriggerFilterCount DEPRECATED_MSG_ATTRIBUTE("use dumpDailyLimit instead");
- (uint32_t)getDumpDailyLimit;
- (BOOL)getShouldPrintMemoryUse;
- (BOOL)getShouldGetDiskIOStack DEPRECATED_MSG_ATTRIBUTE("feature removed");
- (size_t)getSingleReadLimit DEPRECATED_MSG_ATTRIBUTE("feature removed");
- (size_t)getSingleWriteLimit DEPRECATED_MSG_ATTRIBUTE("feature removed");
- (size_t)getTotalReadLimit DEPRECATED_MSG_ATTRIBUTE("feature removed");
- (size_t)getTotalWriteLimit DEPRECATED_MSG_ATTRIBUTE("feature removed");
- (BOOL)getShouldPrintCPUFrequency;
- (uint32_t)getMemoryWarningThresholdInMB;
- (BOOL)getSensitiveRunloopHangDetection;
- (BOOL)getShouldSuspendAllThreads;
- (BOOL)getShouldEnableSnapshot;

@end
