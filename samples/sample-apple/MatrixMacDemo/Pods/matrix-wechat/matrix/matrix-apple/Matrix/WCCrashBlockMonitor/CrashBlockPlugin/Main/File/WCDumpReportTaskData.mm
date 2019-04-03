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

#import "WCDumpReportTaskData.h"

#define IntNumberObject(Name) [NSNumber numberWithInt:Name]

const NSDictionary *WXGDumpReportTypeConfig = @{
                                                IntNumberObject(EDumpType_BlockAndBeKilled) : IntNumberObject(EReportPriority_High),
                                                IntNumberObject(EDumpType_MainThreadBlock) : IntNumberObject(EReportPriority_High),
                                                IntNumberObject(EDumpType_BlockThreadTooMuch) : IntNumberObject(EReportPriority_High),
                                                IntNumberObject(EDumpType_LaunchBlock) : IntNumberObject(EReportPriority_High),
                                                IntNumberObject(EDumpType_CPUIntervalHigh) : IntNumberObject(EReportPriority_Default),
                                                IntNumberObject(EDumpType_BackgroundMainThreadBlock) : IntNumberObject(EReportPriority_Low),
                                                IntNumberObject(EDumpType_CPUBlock) : IntNumberObject(EReportPriority_Low),
                                                };

@implementation WCDumpReportTaskData

@synthesize m_uploadFilesArray;
@synthesize m_dumpType;

- (NSString *)description
{
    return [NSString stringWithFormat:@"Task info: dumpType[%lu] fileCount[%lu]", (unsigned long)m_dumpType, (unsigned long)[m_uploadFilesArray count]];
}

@end
