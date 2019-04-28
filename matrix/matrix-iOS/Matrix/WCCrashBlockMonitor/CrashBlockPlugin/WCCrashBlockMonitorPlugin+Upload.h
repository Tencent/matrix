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

#import "WCCrashBlockMonitorPlugin.h"

@interface WCCrashBlockMonitorPlugin (Upload)

// ============================================================================
#pragma mark - Crash
// ============================================================================

/**
 * @brief report the crash
 */
- (void)reportCrash;

/// detect if has crash file or not
+ (BOOL)hasCrashReport;

// ============================================================================
#pragma mark - Lag & Lag File
// ============================================================================

+ (BOOL)haveLagFiles;
+ (BOOL)haveLagFilesOnDate:(NSString *)nsDate;
+ (BOOL)haveLagFilesOnType:(EDumpType)dumpType;
+ (BOOL)haveLagFilesOnDate:(NSString *)nsDate onType:(EDumpType)dumpType;

// Importantly, the use of reportCntLimit.
// use a little count of reportCntLimit to seperate the MatrixIssue,
// then we can decrease the data size of one MatrixIssue,
// then we can speed up the upload of one MatrixIssue and increase the success rate of upload
// default to 3.

/**
 * @brief report all type of lag file,
 * One MatrixIssue must have only one type of lag file.
 */
- (void)reportAllLagFile;

/**
 * @brief report all type of lag file on the specified date
 * @param nsDate the given date (format: yyyy-MM-dd)
 */
- (void)reportAllLagFileOnDate:(NSString *)nsDate;

/**
 * @brief report a specified type of lag on the given data.
 * @param dumpType given type
 * @param nsDate given date
 */
- (void)reportOneTypeLag:(EDumpType)dumpType
                  onDate:(NSString *)nsDate;

@end
