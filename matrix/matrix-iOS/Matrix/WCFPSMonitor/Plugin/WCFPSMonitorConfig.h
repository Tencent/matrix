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

#import "MatrixPluginConfig.h"

typedef NS_ENUM(NSUInteger, MMFPSTrackingType) {
    MMFPSTrackingType_None = 0,
    MMFPSTrackingType_Scroll = 1,
    MMFPSTrackingType_Transition = 2,
};

@interface WCFPSMonitorConfig : MatrixPluginConfig

+ (WCFPSMonitorConfig *)defaultConfigurationForScroll;
+ (WCFPSMonitorConfig *)defaultConfigurationForTransition;

@property (nonatomic, assign) MMFPSTrackingType trackingType;

@property (nonatomic, assign) BOOL autoUpload;
@property (nonatomic, assign) int dumpInterval; // 主线程堆栈捕获间隔，单位 ms
@property (nonatomic, assign) int dumpMaxCount; // 堆栈最大捕获数量
@property (nonatomic, assign) double maxFrameInterval; // 两帧时间超过这个阀值累计延迟值
@property (nonatomic, assign) double maxDumpTimestamp; // 连续 N 帧累计延迟值大于 maxDumpTimestamp，保存堆栈
@property (nonatomic, assign) double powFactor; // 掉帧时加权系数

@end
