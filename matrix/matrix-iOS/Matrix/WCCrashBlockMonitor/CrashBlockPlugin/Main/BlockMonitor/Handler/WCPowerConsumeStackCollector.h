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
#import "KSStackCursor_Backtrace.h"

// ============================================================================
#pragma mark - WCAddressFrame
// ============================================================================

@interface WCAddressFrame : NSObject

- (id)initWithAddress:(uintptr_t)address withRepeatCount:(uint32_t)count;
- (void)addChildFrame:(WCAddressFrame *)addressFrame;

@end

// ============================================================================
#pragma mark - WCStackTracePool
// ============================================================================

@interface WCStackTracePool : NSObject

- (id)initWithMaxStackTraceCount:(NSUInteger)maxStackTraceCount;
- (void)addThreadStack:(uintptr_t *)stackArray andLength:(size_t)stackCount andCPU:(float)stackCPU;
- (NSArray <NSDictionary *>*)makeCallTree;

@end

// ============================================================================
#pragma mark - WCPowerConsumeStackCollector
// ============================================================================

@protocol WCPowerConsumeStackCollectorDelegate <NSObject>

- (void)powerConsumeStackCollectorConclude:(NSArray <NSDictionary *>*)stackTree;

@end

@interface WCPowerConsumeStackCollector : NSObject

@property (nonatomic, weak) id<WCPowerConsumeStackCollectorDelegate> delegate;

- (id)initWithCPULimit:(float)cpuLimit;

/** generate the power consume stack tree **/
- (void)makeConclusion;

- (float)getCPUUsageAndPowerConsumeStack;

/** check cpu high block **/
- (BOOL)isCPUHighBlock;

/** get cpu high block stack size **/
- (int)getCurrentCpuHighStackNumber;

/** get cpu high block stack cursor **/
- (KSStackCursor **)getCPUStackCursor;

/** get cpu high thread value array **/
- (float *)getCpuHighThreadValueArray;

/**
 *  get current CPU usage
 *  @return current cpu usage. Single core is full 100%, dual core is full 200%, and so on.
 */
+ (float)getCurrentCPUUsage;

@end
