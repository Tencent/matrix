/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "WCFPSMonitorConfig.h"

@implementation WCFPSMonitorConfig

+ (WCFPSMonitorConfig *)defaultConfigurationForScroll {
    WCFPSMonitorConfig *config = [[WCFPSMonitorConfig alloc] init];
    config.trackingType = MMFPSTrackingType_Scroll;
    config.autoUpload = YES;
    config.dumpInterval = 1;
    config.dumpMaxCount = 500;
    config.maxFrameInterval = 1000 / 25;
    config.maxDumpTimestamp = 120;
    config.powFactor = 2.5;
    return config;
}

+ (WCFPSMonitorConfig *)defaultConfigurationForTransition {
    WCFPSMonitorConfig *config = [[WCFPSMonitorConfig alloc] init];
    config.trackingType = MMFPSTrackingType_Transition;
    config.autoUpload = NO;
    config.dumpInterval = 2;
    config.dumpMaxCount = 500;
    config.maxFrameInterval = 1000 / 25;
    config.maxDumpTimestamp = 800;
    config.powFactor = 2.5;
    return config;
}

@end
