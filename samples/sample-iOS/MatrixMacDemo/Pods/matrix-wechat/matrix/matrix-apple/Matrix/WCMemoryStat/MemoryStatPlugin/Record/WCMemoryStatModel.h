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

#import "MatrixBaseModel.h"

// record the thread info
@interface MSThreadInfo : MatrixBaseModel

@property (nonatomic, strong) NSString *threadName;
@property (nonatomic, assign) UInt64 allocSize;

@end

@interface MemoryRecordInfo : MatrixBaseModel

@property (nonatomic, assign) uint64_t launchTime;
@property (nonatomic, strong) NSString *userScene;
@property (nonatomic, strong) NSString *systemVersion;
@property (nonatomic, strong) NSString *appUUID;
@property (nonatomic, strong) NSMutableDictionary<NSString *, MSThreadInfo *> *threadInfos;

- (NSString *)recordID;
- (NSString *)recordDataPath;

- (NSData *)generateReportDataWithCustomInfo:(NSDictionary *)customInfo;

- (void)calculateAllThreadsMemoryUsage;

@end
