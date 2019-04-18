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
#import "MatrixPluginConfig.h"

#import "WCBlockMonitorConfiguration.h"
#import "WCCrashBlockMonitorDelegate.h"
#import "KSCrashReportWriter.h"

typedef NS_ENUM(NSUInteger, EWCCrashBlockReportStrategy) {
    EWCCrashBlockReportStrategy_Auto = 0, 
    EWCCrashBlockReportStrategy_All = 1,
    EWCCrashBlockReportStrategy_Manual = 2,
};

@interface WCCrashBlockMonitorConfig : MatrixPluginConfig

+ (WCCrashBlockMonitorConfig *)defaultConfiguration;

/// set the app's full version, exp. 6.6.6.6
@property (nonatomic, copy) NSString *appVersion;

/// set the app's short version, exp. 6.6.6
@property (nonatomic, copy) NSString *appShortVersion;

/// shoule enable crash
@property (nonatomic, assign) BOOL enableCrash;

/// should enable the block monitor
@property (nonatomic, assign) BOOL enableBlockMonitor;

/**
 * @brief callback of the BlockMonitor
 * use this delegate to get more detail of the running of BlockMonitor
 */
@property (nonatomic, weak) id<WCCrashBlockMonitorDelegate> blockMonitorDelegate;

/// after crash, callback to handle the deadly signals
@property (nonatomic, readwrite, assign) KSCrashSentryHandleSignal onHandleSignalCallBack;

/**
 * @brief use this callback to write you own info into the crash file and the lag file,
 * note, not only the crash file
 */
@property (nonatomic, readwrite, assign) KSReportWriteCallback onAppendAdditionalInfoCallBack;

/// configuration of the block monitor
@property (nonatomic, strong) WCBlockMonitorConfiguration *blockMonitorConfiguration;

@property (nonatomic, assign) EWCCrashBlockReportStrategy reportStrategy;

@end
