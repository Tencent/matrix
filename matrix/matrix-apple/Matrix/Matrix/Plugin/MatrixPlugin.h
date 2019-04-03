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

#import "MatrixAdapter.h"
#import "MatrixIssue.h"
#import "MatrixPluginConfig.h"

// ============================================================================
#pragma mark - MatrixPluginListenerDelegate
// ============================================================================

@protocol MatrixPluginProtocol;

// Define what a listener can listen
@protocol MatrixPluginListenerDelegate <NSObject>

@optional

- (void)onInit:(id<MatrixPluginProtocol>)plugin;
- (void)onStart:(id<MatrixPluginProtocol>)plugin;
- (void)onStop:(id<MatrixPluginProtocol>)plugin;
- (void)onDestroy:(id<MatrixPluginProtocol>)plugin;

@required
/**
 * @brief The issue that has been triggered to report
 * The listener has to help to report the issue to the right place
 *
 * @param issue The issue which be triggered to be reported
 */
- (void)onReportIssue:(MatrixIssue *)issue;

@end

// ============================================================================
#pragma mark - MatrixPluginProtocol
// ============================================================================

// Define what a matrix plugin
@protocol MatrixPluginProtocol <NSObject>

@required

- (void)start;

- (void)stop;

- (void)destroy;

/**
 * @brief the listener who want to know about what happen in the plugin.
 * @param pluginListener The listener.
 */
- (void)setupPluginListener:(id<MatrixPluginListenerDelegate>)pluginListener;

/***
 * @brief report matrix issue. advise: you should implement this reportIssue in your own plguin.
 * After call reportIssue, this method will call the listener onReportIssue.
 * @param issue The issue you want to be reported
 */
- (void)reportIssue:(MatrixIssue *)issue;

/**
 * @brief tell the plugin the issue has been handled completely.
 * @param issue The issue was been handled.
 * @param bSuccess The result of handling
 */
- (void)reportIssueCompleteWithIssue:(MatrixIssue *)issue success:(BOOL)bSuccess;

/**
 * @brief get the tag of current plugin. A tag is a unique indentifier of plugin.
 * @return NSString * Tag of current plugin.
 */
+ (NSString *)getTag;

@end

// ============================================================================
#pragma mark - MatrixPlugin
// ============================================================================

@interface MatrixPlugin : NSObject <MatrixPluginProtocol>

@property (nonatomic, strong) MatrixPluginConfig *pluginConfig;

@end
