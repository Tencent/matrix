/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

NS_ASSUME_NONNULL_BEGIN

// ============================================================================
#pragma mark - WCAddressFrame
// ============================================================================

@interface WCAddressFrame : NSObject

@property (nonatomic, assign) uintptr_t address;
@property (nonatomic, assign) uint32_t repeatCount;
@property (nonatomic, assign) uint32_t repeatCountInBackground;
@property (nonatomic, strong) NSMutableArray<WCAddressFrame *> *childAddressFrame;

- (id)initWithAddress:(uintptr_t)address withRepeatCount:(uint32_t)count isInBackground:(BOOL)isInBackground;
- (void)addChildFrame:(WCAddressFrame *)addressFrame;
- (void)symbolicate;
- (NSDictionary *)getInfoDict;
- (WCAddressFrame *)tryFoundAddressFrameWithAddress:(uintptr_t)toFindAddress;

@end

@interface WCMatrixModel : NSObject

@end

NS_ASSUME_NONNULL_END
