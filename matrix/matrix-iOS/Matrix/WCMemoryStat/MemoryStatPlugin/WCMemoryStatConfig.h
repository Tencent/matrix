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

#import "MatrixPluginConfig.h"

typedef NS_ENUM(NSUInteger, EWCMemStatReportStrategy) {
    EWCMemStatReportStrategy_Auto = 0,
    EWCMemStatReportStrategy_Manual = 1,
};

@interface WCMemoryStatConfig : MatrixPluginConfig

+ (WCMemoryStatConfig *)defaultConfiguration;

/**
 * The filtering strategy of the stack
 */

// If the malloc size more than 'skipMinMallocSize', the stack will be saved. Default to PAGE_SIZE
@property (nonatomic, assign) int skipMinMallocSize;
// Otherwise if the stack contains App's symbols in the last 'skipMaxStackDepth' address,
// the stack also be saved. Default to 8
@property (nonatomic, assign) int skipMaxStackDepth;


@property (nonatomic, assign) EWCMemStatReportStrategy reportStrategy;

@end
