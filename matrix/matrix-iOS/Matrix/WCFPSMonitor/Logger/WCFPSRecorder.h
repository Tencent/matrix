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

// ============================================================================
#pragma mark - WCFPSRecorder
// ============================================================================

@interface WCFPSRecorder : NSObject

@property (nonatomic, assign) int recordID;
@property (nonatomic, assign) int dumpInterval; // ms
@property (nonatomic, assign) double dumpTimeBegin;
@property (nonatomic, assign) double dumpTimeTotal;
@property (nonatomic, assign) BOOL shouldDump;
@property (nonatomic, assign) BOOL shouldDumpCpuUsage;
@property (nonatomic, assign) BOOL shouldExit;
@property (nonatomic, assign) BOOL autoUpload;
@property (nonatomic, strong) NSString *scene;
@property (nonatomic, strong) NSString *reportID;
@property (nonatomic, strong) NSString *reportPath;

- (id)initWithMaxStackTraceCount:(uint32_t)maxStackTraceCount timeInterval:(uint32_t)ti;
- (void)addThreadStack:(uintptr_t *)stackArray andLength:(size_t)stackCount;
- (NSArray<NSDictionary *> *)makeCallTree;
//- (NSString *)makeCallTotalString;

@end
