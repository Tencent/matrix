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

@interface WCDumpInterface : NSObject

/**
 *  @brief dump custom crash report (which has current thread state, etc) without abort
 *  @param dumpType the report type of this report
 *  @param blockTime the block time which defined by developer (can use to measure)
 *  @param exceptionReason the reason why the report generated
 *  @param bSelfDefined use the self defined path or not, default is true.
 *  @return NSString * the file path of the crash report
 */
+ (NSString *)dumpReportWithReportType:(EDumpType)dumpType
                         withBlockTime:(uint64_t)blockTime
                   withExceptionReason:(NSString *)exceptionReason
                       selfDefinedPath:(BOOL)bSelfDefined;

+ (NSString *)dumpReportWithReportType:(EDumpType)dumpType
                         withBlockTime:(uint64_t)blockTime
                   withExceptionReason:(NSString *)exceptionReason;

/**
 *  @brief dump custom crash report (which has current thread state, etc) without abort
 *  @param dumpType the report type of this report
 *  @param blockTime the block time which defined by developer (can use to measure)
 *  @return NSString * the file path of the crash report
 */
+ (NSString *)dumpReportWithReportType:(EDumpType)dumpType
                         withBlockTime:(uint64_t)blockTime;

/**
 *  @brief save dump data to file
 *  @param dumpData the dump data
 *  @param dumpType the dump type of this data
 *  @param reportID the reportID of this data
 */
+ (NSString *)saveDump:(NSData *)dumpData
        withReportType:(EDumpType)dumpType
          withReportID:(NSString *)reportID;

@end
