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
#import "WCBlockTypeDef.h"

@interface WCCrashBlockFileHandler : NSObject

// ============================================================================
#pragma mark - Crash File
// ============================================================================

+ (BOOL)hasCrashReport;

+ (NSDictionary *)getPendingCrashReportInfo;

+ (void)deleteCrashDataWithReportID:(NSString *)reportID;

+ (NSArray *)getAllCrashReportPath;
+ (NSArray *)getAllCrashReportID;

// ============================================================================
#pragma mark - Lag File
// ============================================================================

+ (BOOL)haveLagFiles;
+ (BOOL)haveLagFilesOnDate:(NSString *)nsDate;
+ (BOOL)haveLagFilesOnType:(EDumpType)dumpType;
+ (BOOL)haveLagFilesOnDate:(NSString *)nsDate onType:(EDumpType)dumpType;

+ (NSString *)diretoryOfUserDump;
+ (NSString *)diretoryOfUserDumpWithType:(EDumpType)type;

+ (NSArray *)getLagReportIDWithType:(EDumpType)dumpType withDate:(NSString *)limitDate;
+ (NSData *)getLagDataWithReportID:(NSString *)reportID andReportType:(EDumpType)dumpType;
+ (NSData *)getLagDataWithReportIDArray:(NSArray *)reportIDArray andReportType:(EDumpType)dumpType;

+ (void)deleteLagDataWithReportID:(NSString *)reportID andReportType:(EDumpType)dumpType;

// ============================================================================
#pragma mark - Launch Lag Info
// ============================================================================

+ (NSString *)getLaunchBlockRecordFilePath;

// ============================================================================
#pragma mark - Stack Feat
// ============================================================================

+ (NSString *)getStackFeatFilePath;

// ============================================================================
#pragma mark - Handle OOM
// ============================================================================

+ (void)handleOOMDumpFile:(NSString *)lagFilePath;

@end
