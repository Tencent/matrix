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
#import "MatrixFramework.h"
#import "MatrixAppRebootType.h"
#import "WCCrashBlockMonitorPlugin.h"
#import "WCCrashBlockMonitorPlugin+Upload.h"
#import "WCMemoryStatPlugin.h"

@class MatrixPlugin;
@protocol MatrixPluginListenerDelegate;

@interface MatrixBuilder : NSObject

@property (nonatomic, weak) id<MatrixPluginListenerDelegate> pluginListener;

- (void)addPlugin:(MatrixPlugin *)plugin;

- (NSMutableSet *)getPlugins;

@end

@interface Matrix : NSObject

+ (id)sharedInstance;

- (void)addMatrixBuilder:(MatrixBuilder *)builder;

- (void)startPlugins;
- (void)stopPlugins;

- (NSMutableSet *)getPlugins;
- (MatrixPlugin *)getPluginWithTag:(NSString *)tag;

- (void)reportIssueComplete:(MatrixIssue *)matrixIssue success:(BOOL)bSuccess;

// ============================================================================
#pragma mark - AppReboot
// ============================================================================

/**
 *  The Matrix has built-in functionality to detect the type of last launch.
 */

/// the reboot type of the current launch, use this to know the reason that the app was killed last time.
- (MatrixAppRebootType)lastRebootType;

/// the timestamp of current launch.
- (uint64_t)appLaunchTime;

/// the timestamp of last launch.
- (uint64_t)lastAppLaunchTime;

/// can set the user scene constantly, and use the -[Matrix userSceneOfLastRun] to get the scene set lastly
- (void)setUserScene:(NSString *)scene;

/// get the last scene that set in last app running.
- (NSString *)userSceneOfLastRun;


#if !TARGET_OS_OSX

/// notify the app is in Background Fetch, help improve the detection of the plugins
- (void)notifyAppBackgroundFetch;

/// notify the app will suspend, help improve the detection of the plugins
- (void)notifyAppWillSuspend;

#endif

@end
