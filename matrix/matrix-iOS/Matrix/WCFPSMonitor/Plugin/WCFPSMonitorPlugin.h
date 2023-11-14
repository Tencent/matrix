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

#import "Matrix.h"
#import "WCFPSMonitorConfig.h"
#import "WCReportStrategyDelegate.h"

@class WCFPSMonitorPlugin;

@protocol WCFPSMonitorPluginDelegate <NSObject>

// ms
- (void)onFPSMonitorPluginReport:(WCFPSMonitorPlugin *)plugin totalTime:(double)totalTime dropFrameTime:(double)dropFrameTime scene:(NSString *)scene;

@end

@interface WCFPSMonitorPlugin : MatrixPlugin

@property (nonatomic, strong) WCFPSMonitorConfig *pluginConfig;
@property (nonatomic, weak) id<WCFPSMonitorPluginDelegate> delegate;
@property (nonatomic, weak) id<WCReportStrategyDelegate> reportDelegate;

- (void)startDisplayLink:(NSString *)scene;
- (void)stopDisplayLink;

@end
