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

@interface WCCPUHandler : NSObject

- (BOOL)cultivateCpuUsage:(float)cpuUsage periodTime:(float)periodSec;

/**
 * if the CPU occupied too little when it is in the background
 * @return BOOL
 */
- (BOOL)isBackgroundCPUTooSmall;

/**
 *  get current CPU usage
 *
 *  @return current cpu usage. Single core is full 100%, dual core is full 200%, and so on.
 */
+ (float)getCurrentCpuUsage;


@end
