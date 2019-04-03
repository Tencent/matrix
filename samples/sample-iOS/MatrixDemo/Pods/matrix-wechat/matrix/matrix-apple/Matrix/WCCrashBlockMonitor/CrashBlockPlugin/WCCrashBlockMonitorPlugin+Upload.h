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

- (void)reportAllLagFileWithLimitReportCountInOneIssue:(NSUInteger)reportCntLimit;

/**
 * @brief report all type of lag file on the specified date
 * @param nsDate the given date (format: yyyy-MM-dd)
 */
- (void)reportAllLagFileOnDate:(NSString *)nsDate;

- (void)reportAllLagFileOnDate:(NSString *)nsDate
withLimitReportCountInOneIssue:(NSUInteger)reportCntLimit;

/**
 * @brief report a specified type of lag on the given data.
 * @param dumpType given type
 * @param nsDate given date
 */
- (void)reportOneTypeLag:(EDumpType)dumpType
                  onDate:(NSString *)nsDate;

- (void)reportOneTypeLag:(EDumpType)dumpType
                  onDate:(NSString *)nsDate
withLimitReportCountInOneIssue:(NSUInteger)reportCntLimit;


// Importantly, the method will report just one type of lag at today one time.
// The reporting of lag files is in order, the high priority will be reported firstly,
// If you care about the priority, see WCDumpReportTypeTaskData's WXGDumpReportTypeConfig.

/**
 * @brief report the lag generated today with the limit type configuration
 * @param limitConfigStr The type limited to report
 * note.
 * according to EDumpType, if set the limitConfigStr = @"2001,2002",
 * EDumpType_KS_MainThreadBlock(2001),EDumpType_KS_BackgroundMainThreadBlock(2002) would not be reported
 * if set the limitConfig = @"" or nil, then none is limited
 */
- (void)reportTodayOneTypeLagWithlimitUploadConfig:(NSString *)limitConfigStr;

- (void)reportTodayOneTypeLagWithlimitUploadConfig:(NSString *)limitConfigStr
                    withLimitReportCountInOneIssue:(NSUInteger)reportCntLimit;

- (void)reportTodayOneTypeLag;


@end
