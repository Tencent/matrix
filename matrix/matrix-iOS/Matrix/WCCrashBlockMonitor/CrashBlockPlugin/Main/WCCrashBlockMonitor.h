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

#import "KSCrash.h"
#import "WCBlockTypeDef.h"
#import "WCBlockMonitorConfiguration.h"
#import "WCCrashBlockMonitorDelegate.h"

@class CrashBlockMonitorPlugin;

@interface WCCrashBlockMonitor : NSObject

/// app's full versionl, exp: 6.6.6.6
@property (nonatomic, copy) NSString *appVersion;

/// app's short version, exp: 6.6.6
@property (nonatomic, copy) NSString *appShortVersion;

/// callback after having caught a deadly signal.
@property (nonatomic, readwrite, assign) KSCrashSentryHandleSignal onHandleSignalCallBack;

/// callback after crash, use to add extra crash info.
@property (nonatomic, readwrite, assign) KSReportWriteCallback onAppendAdditionalInfoCallBack;

@property (nonatomic, weak) id<WCCrashBlockMonitorDelegate> delegate;

/// BlockMonitor's configuration
@property (nonatomic, strong) WCBlockMonitorConfiguration *bmConfiguration;

- (BOOL)installKSCrash:(BOOL)shouldCrash;

- (void)enableBlockMonitor;
- (void)startBlockMonitor;
- (void)stopBlockMonitor;

// use this method to refresh the version info in the dump file
- (void)resetAppFullVersion:(NSString *)fullVersion shortVersion:(NSString *)shortVersion;

#if !TARGET_OS_OSX

// for optimization, for reduce misinformation

- (void)handleBackgroundLaunch;
- (void)handleSuspend;

#endif

// control the CPU tracking

- (void)startTrackCPU;
- (void)stopTrackCPU;

// get the CPU cost in background
- (BOOL)isBackgroundCPUTooSmall;

// generate stack snapshot
- (void)generateLiveReportWithDumpType:(EDumpType)dumpType withReason:(NSString *)reason selfDefinedPath:(BOOL)bSelfDefined;

@end
