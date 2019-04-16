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

#import "WCDumpReportDataProvider.h"
#import "WCCrashBlockFileHandler.h"

@implementation WCDumpReportDataProvider

// ============================================================================
#pragma mark - Interface
// ============================================================================

+ (WCDumpReportTaskData *)getTodayOneReportDataWithLimitType:(NSString *)limitTypeString
{
    NSMutableArray *arrPriorityHigh = [[NSMutableArray alloc] init];
    NSMutableArray *arrPriorityDefault = [[NSMutableArray alloc] init];
    NSMutableArray *arrPriorityLow = [[NSMutableArray alloc] init];

    for (NSNumber *type in WXGDumpReportTypeConfig) {
        if (type == nil) {
            break;
        }
        NSNumber *numPriority = WXGDumpReportTypeConfig[type];
        EReportPriority priority = (EReportPriority)[numPriority intValue];
        NSString *tmpDumpTypeString = [NSString stringWithFormat:@"%lu", (unsigned long) [type unsignedIntegerValue]];
        if (tmpDumpTypeString && [tmpDumpTypeString length] > 0 && limitTypeString && [limitTypeString length] > 0) {
            if ([limitTypeString containsString:tmpDumpTypeString]) {
                continue;
            }
        }
        switch (priority) {
            case EReportPriority_High:
                [arrPriorityHigh addObject:type];
                break;
            case EReportPriority_Default:
                [arrPriorityDefault addObject:type];
                break;
            case EReportPriority_Low:
                [arrPriorityLow addObject:type];
                break;
            default:
                break;
        }
    }

    NSDate *date = [NSDate date];
    static NSDateFormatter *formatter = nil;
    if (!formatter) {
        formatter = [[NSDateFormatter alloc] init];
        formatter.dateFormat = @"yyyy-MM-dd";
    }
    NSString *nsDate = [formatter stringFromDate:date];

    WCDumpReportTaskData *retReportData = nil;
    for (NSNumber *type in arrPriorityHigh) {
        retReportData = [WCDumpReportDataProvider getDumpReportDataWithDumpType:(EDumpType)[type intValue] withDate:nsDate];
        if (retReportData && retReportData.m_uploadFilesArray.count) {
            return retReportData;
        }
    }

    for (NSNumber *type in arrPriorityDefault) {
        retReportData = [WCDumpReportDataProvider getDumpReportDataWithDumpType:(EDumpType)[type intValue] withDate:nsDate];
        if (retReportData && retReportData.m_uploadFilesArray.count) {
            return retReportData;
        }
    }

    for (NSNumber *type in arrPriorityLow) {
        retReportData = [WCDumpReportDataProvider getDumpReportDataWithDumpType:(EDumpType)[type intValue] withDate:nsDate];
        if (retReportData && retReportData.m_uploadFilesArray.count) {
            return retReportData;
        }
    }

    return retReportData;
}

+ (NSArray <WCDumpReportTaskData *>*)getAllTypeReportDataWithDate:(NSString *)limitDate
{
    NSMutableArray <WCDumpReportTaskData *>*arrReportData = [[NSMutableArray alloc] init];
    NSArray *keyArray = [WXGDumpReportTypeConfig allKeys];

    for (NSNumber *reportTypeNum in keyArray) {
        EDumpType dumpType = (EDumpType)[reportTypeNum intValue];
        WCDumpReportTaskData *dumpReportData;

        if (limitDate.length == 0 || limitDate == NULL) {
            dumpReportData = [WCDumpReportDataProvider getDumpReportDataWithDumpType:dumpType];
        } else {
            dumpReportData = [WCDumpReportDataProvider getDumpReportDataWithDumpType:dumpType withDate:limitDate];
        }

        if ([dumpReportData.m_uploadFilesArray count] > 0) {
            [arrReportData addObject:dumpReportData];
        }
    }

    return [arrReportData copy];
}

+ (NSArray <WCDumpReportTaskData *>*)getReportDataWithType:(EDumpType)dumpType onDate:(NSString *)nsDate
{
    NSMutableArray *arrReportData = [[NSMutableArray alloc] init];

    WCDumpReportTaskData *dumpReportData;

    if (nsDate.length == 0 || nsDate == nil) {
        dumpReportData = [WCDumpReportDataProvider getDumpReportDataWithDumpType:dumpType];
    } else {
        dumpReportData = [WCDumpReportDataProvider getDumpReportDataWithDumpType:dumpType withDate:nsDate];
    }
    if ([dumpReportData.m_uploadFilesArray count] > 0) {
        [arrReportData addObject:dumpReportData];
    }

    return arrReportData;
}

// ============================================================================
#pragma mark - Utility
// ============================================================================

+ (WCDumpReportTaskData *)getDumpReportDataWithDumpType:(EDumpType)dumpType
{
    return [WCDumpReportDataProvider getDumpReportDataWithDumpType:dumpType withDate:@""];
}

+ (WCDumpReportTaskData *)getDumpReportDataWithDumpType:(EDumpType)dumpType withDate:(NSString *)limitDate
{
    WCDumpReportTaskData *reportData = [[WCDumpReportTaskData alloc] init];
    reportData.m_uploadFilesArray = [[WCCrashBlockFileHandler getLagReportIDWithType:dumpType withDate:limitDate] mutableCopy];
    reportData.m_dumpType = dumpType;
    return reportData;
}

@end
