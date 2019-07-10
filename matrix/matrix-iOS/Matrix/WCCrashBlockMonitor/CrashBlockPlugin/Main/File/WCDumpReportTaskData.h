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

// define the report priority.
typedef NS_ENUM(NSUInteger, EReportPriority) {
    EReportPriority_Low = 0,
    EReportPriority_Default = 1,
    EReportPriority_High = 2,
};

// important, there will be only one type of lag in WCDumpReportTaskData.
@interface WCDumpReportTaskData : NSObject

@property (nonatomic, strong) NSMutableArray *m_uploadFilesArray;
@property (nonatomic, assign) EDumpType m_dumpType;

@end


// define the report type and the report priority,
// if adding one new lag type and want to be reported,
// you must add the lag type in this report configuration.
extern const NSDictionary *WXGDumpReportTypeConfig;
