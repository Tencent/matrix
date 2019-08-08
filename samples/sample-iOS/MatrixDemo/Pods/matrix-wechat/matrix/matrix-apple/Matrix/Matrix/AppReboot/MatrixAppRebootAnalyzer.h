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
#import "MatrixBaseModel.h"
#import "MatrixAppRebootType.h"

// ============================================================================
#pragma mark - MatrixAppRebootInfo
// ============================================================================

@interface MatrixAppRebootInfo : MatrixBaseModel

@property (nonatomic, assign) BOOL isAppEnterForeground;
@property (nonatomic, assign) BOOL isAppEnterBackground;
@property (nonatomic, assign) BOOL isAppWillSuspend;
@property (nonatomic, assign) BOOL isAppBackgroundFetch;
@property (nonatomic, assign) BOOL isAppSuspendKilled;
@property (nonatomic, assign) BOOL isAppCrashed;
@property (nonatomic, assign) BOOL isAppQuitByExit;
@property (nonatomic, assign) BOOL isAppQuitByUser;
@property (nonatomic, assign) BOOL isAppMainThreadBlocked;
@property (nonatomic, assign) uint64_t appLaunchTime;
@property (nonatomic, strong) NSString *appUUID;
@property (nonatomic, strong) NSString *osVersion;
@property (nonatomic, strong) NSString *dumpFileName;
@property (nonatomic, strong) NSString *userScene;

+ (MatrixAppRebootInfo *)sharedInstance;
- (void)saveInfo;

@end

// ============================================================================
#pragma mark - MatrixAppRebootAnalyzer
// ============================================================================

@interface MatrixAppRebootAnalyzer : NSObject

+ (void)installAppRebootAnalyzer;
+ (void)checkRebootType;
+ (void)notifyAppCrashed;
+ (void)isForegroundMainThreadBlock:(BOOL)yn;
+ (void)setDumpFileName:(NSString *)fileName;
+ (NSString *)lastDumpFileName;

+ (MatrixAppRebootType)lastRebootType;
+ (uint64_t)appLaunchTime;
+ (uint64_t)lastAppLaunchTime;

+ (void)notifyAppBackgroundFetch;
+ (void)notifyAppWillSuspend;
+ (void)setUserScene:(NSString *)scene;
+ (NSString *)userSceneOfLastRun;

@end
