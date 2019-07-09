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
#import <TargetConditionals.h>

#import "WCBlockTypeDef.h"
#import "WCMainThreadHandler.h"

@class WCBlockMonitorConfiguration;
@class WCBlockMonitorMgr;

@protocol WCBlockMonitorDelegate <NSObject>

@required

- (void)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr enterNextCheckWithDumpType:(EDumpType)dumpType;
- (void)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr beginDump:(EDumpType)dumpType blockTime:(uint64_t)blockTime;
- (void)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr dumpType:(EDumpType)dumpType filter:(EFilterType)filterType;
- (void)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr getDumpFile:(NSString *)dumpFile withDumpType:(EDumpType)dumpType;
- (NSDictionary *)onBlockMonitor:(WCBlockMonitorMgr *)bmMgr getUserInfoForFPSDumpWithDumpType:(EDumpType)dumpType;
- (void)onBlockMonitorCurrentCPUTooHigh:(WCBlockMonitorMgr *)bmMgr;
- (void)onBlockMonitorIntervalCPUTooHigh:(WCBlockMonitorMgr *)bmMgr;

@end

// Get the most time-consuming stack recently
KSStackCursor *kscrash_pointThreadCallback(void);

@interface WCBlockMonitorMgr : NSObject

@property (nonatomic, weak) id<WCBlockMonitorDelegate> delegate;

+ (WCBlockMonitorMgr *)shareInstance;

- (void)resetConfiguration:(WCBlockMonitorConfiguration *)bmConfig;
- (void)start;
- (void)stop;

// ============================================================================
#pragma mark - Optimization
// ============================================================================

#if !TARGET_OS_OSX

/// Voip Push or BackgroundFetch trigger the app launch
- (void)handleBackgroundLaunch;

- (void)handleSuspend;

#endif

// ============================================================================
#pragma mark - CPU
// ============================================================================

- (void)startTrackCPU;
- (void)stopTrackCPU;
- (BOOL)isBackgroundCPUTooSmall;

// ============================================================================
#pragma mark - Utility
// ============================================================================

- (NSDictionary *)getUserInfoForCurrentDumpForDumpType:(EDumpType)dumpType;

#if TARGET_OS_OSX
+ (void)signalEventStart;
+ (void)signalEventEnd;
#endif

@end
