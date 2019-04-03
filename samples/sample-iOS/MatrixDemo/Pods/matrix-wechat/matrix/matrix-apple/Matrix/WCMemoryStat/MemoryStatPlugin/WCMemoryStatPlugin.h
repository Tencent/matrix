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

#import "Matrix.h"

#import "WCMemoryStatConfig.h"
#import "WCMemoryStatModel.h"
#import "memory_stat_err_code.h"

@class WCMemoryStatPlugin;

@protocol WCMemoryStatPluginDelegate <NSObject>

/// the error code is defined in "memory_stat_err_code.h"
- (void)onMemoryStatPlugin:(WCMemoryStatPlugin *)plugin hasError:(int)errCode;

- (NSDictionary *)onMemoryStatPluginGetCustomInfo:(WCMemoryStatPlugin *)plugin;

@end

@interface WCMemoryStatPlugin : MatrixPlugin

@property (nonatomic, strong) WCMemoryStatConfig *pluginConfig;
@property (nonatomic, weak) id<WCMemoryStatPluginDelegate> delegate;

// ============================================================================
#pragma mark - Report
// ============================================================================

- (MatrixIssue *)uploadReport:(MemoryRecordInfo *)record withCustomInfo:(NSDictionary *)customInfo;

// ============================================================================
#pragma mark - Current Thread
// ============================================================================

/// add a tag to current thread, then you can distinguish it from threads.
- (void)addTagToCurrentThread:(NSString *)tagString;
/// remove the tag of current thread.
- (void)removeTagOfCurrentThread;

- (uint32_t)getMemoryUsageOfCurrentThread;

// ============================================================================
#pragma mark - Record
// ============================================================================

- (NSArray *)recordList;
- (MemoryRecordInfo *)recordOfLastRun;
- (MemoryRecordInfo *)recordByLaunchTime:(uint64_t)launchTime;

- (void)deleteRecord:(MemoryRecordInfo *)record;
- (void)deleteAllRecords;

@end
