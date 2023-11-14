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

#import "WCLagStackTracePoolUtil.h"
#import "WCMatrixModel.h"

#define CHILE_FRAME "child"

@implementation WCLagStackTracePoolUtil

+ (NSArray<NSDictionary *> *)makeCallTreeWithStackCyclePool:(uintptr_t **)stackCyclePool stackCount:(size_t *)stackCount maxStackTraceCount:(NSUInteger)maxStackTraceCount {
    WCLagStackTracePool *pool = [[WCLagStackTracePool alloc] initWithStackCyclePool:stackCyclePool stackCount:stackCount maxStackTraceCount:maxStackTraceCount];
    return [pool makeCallTree];
}

@end

@interface WCLagStackTracePool () {
    uintptr_t **m_stackCyclePool;
    size_t *m_stackCount;
    size_t m_maxStackCount;
}

@property (nonatomic, strong) NSMutableArray<WCAddressFrame *> *parentAddressFrame;

@end

@implementation WCLagStackTracePool

- (id)initWithStackCyclePool:(uintptr_t **)stackCyclePool stackCount:(size_t *)stackCount maxStackTraceCount:(NSUInteger)maxStackTraceCount {
    self = [super init];
    if (self) {
        m_maxStackCount = (size_t)maxStackTraceCount;
        m_stackCyclePool = stackCyclePool;
        m_stackCount = stackCount;
    }
    return self;
}

- (NSArray<NSDictionary *> *)makeCallTree {
    _parentAddressFrame = nil;

    for (int i = 0; i < m_maxStackCount; i++) {
        uintptr_t *curStack = m_stackCyclePool[i];
        size_t curLength = m_stackCount[i];
        WCAddressFrame *curAddressFrame = [self p_getAddressFrameWithStackTraces:curStack length:curLength];
        [self p_addAddressFrame:curAddressFrame];
    }

    NSMutableArray<NSDictionary *> *addressDictArray = [[NSMutableArray alloc] init];

    if ([self.parentAddressFrame count] > 1) {
        [self.parentAddressFrame sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
            WCAddressFrame *frame1 = (WCAddressFrame *)obj1;
            WCAddressFrame *frame2 = (WCAddressFrame *)obj2;
            if (frame1.repeatCount > frame2.repeatCount) {
                return NSOrderedAscending;
            }
            return NSOrderedDescending;
        }];
    }

    for (int i = 0; i < [self.parentAddressFrame count]; i++) {
        WCAddressFrame *addressFrame = self.parentAddressFrame[i];
        [addressFrame symbolicate];
        NSDictionary *curDict = [self p_getInfoDictFromAddressFrame:addressFrame];
        [addressDictArray addObject:curDict];
    }

    return [addressDictArray copy];
}

- (NSDictionary *)p_getInfoDictFromAddressFrame:(WCAddressFrame *)addressFrame {
    NSMutableDictionary *currentInfoDict = [[addressFrame getInfoDict] mutableCopy];
    NSMutableArray<NSDictionary *> *childInfoDict = [[NSMutableArray alloc] init];

    if ([addressFrame.childAddressFrame count] > 1) {
        [addressFrame.childAddressFrame sortUsingComparator:^NSComparisonResult(id _Nonnull obj1, id _Nonnull obj2) {
            WCAddressFrame *frame1 = (WCAddressFrame *)obj1;
            WCAddressFrame *frame2 = (WCAddressFrame *)obj2;
            if (frame1.repeatCount > frame2.repeatCount) {
                return NSOrderedAscending;
            }
            return NSOrderedDescending;
        }];
    }

    for (WCAddressFrame *tmpCallFrame in addressFrame.childAddressFrame) {
        [childInfoDict addObject:[self p_getInfoDictFromAddressFrame:tmpCallFrame]];
    }

    if (childInfoDict != nil && [childInfoDict count] > 0) {
        [currentInfoDict setObject:[childInfoDict copy] forKey:@CHILE_FRAME];
    }
    return [currentInfoDict copy];
}

- (WCAddressFrame *)p_getAddressFrameWithStackTraces:(uintptr_t *)stackTrace length:(size_t)traceLength {
    if (stackTrace == NULL || traceLength == 0) {
        return nil;
    }
    WCAddressFrame *headAddressFrame = nil;
    WCAddressFrame *currentParentFrame = nil;

    for (int i = (int)traceLength - 1; i >= 0; --i) {
        uintptr_t address = stackTrace[i];
        WCAddressFrame *curFrame = [[WCAddressFrame alloc] initWithAddress:address withRepeatCount:1 isInBackground:NO];
        if (currentParentFrame == nil) {
            headAddressFrame = curFrame;
            currentParentFrame = curFrame;
        } else {
            [currentParentFrame addChildFrame:curFrame];
            currentParentFrame = curFrame;
        }
    }
    
    return headAddressFrame;
}

- (void)p_addAddressFrame:(WCAddressFrame *)addressFrame {
    if (addressFrame == nil) {
        return;
    }
    if (_parentAddressFrame == nil) {
        _parentAddressFrame = [[NSMutableArray alloc] init];
    }
    if ([_parentAddressFrame count] == 0) {
        [_parentAddressFrame addObject:addressFrame];
    } else {
        WCAddressFrame *foundAddressFrame = nil;
        for (WCAddressFrame *tmpFrame in _parentAddressFrame) {
            foundAddressFrame = [tmpFrame tryFoundAddressFrameWithAddress:addressFrame.address];
            if (foundAddressFrame != nil) {
                break;
            }
        }
        if (foundAddressFrame == nil) {
            [_parentAddressFrame addObject:addressFrame];
        } else {
            [self p_mergeAddressFrame:foundAddressFrame with:addressFrame];
        }
    }
}

- (void)p_mergeAddressFrame:(WCAddressFrame *)mainFrame with:(WCAddressFrame *)mergedFrame {
    if (mainFrame.address != mergedFrame.address) {
        assert(0);
    }
    mainFrame.repeatCount += mergedFrame.repeatCount;

    if (mainFrame.childAddressFrame == nil || [mainFrame.childAddressFrame count] == 0) {
        mainFrame.childAddressFrame = mergedFrame.childAddressFrame;
        return; // full with mergedFrame.childeAddressFrame
    }

    [self p_mergedAddressFrameArray:mainFrame.childAddressFrame with:mergedFrame.childAddressFrame];
}

- (void)p_mergedAddressFrameArray:(NSMutableArray<WCAddressFrame *> *)mainFrameArray with:(NSMutableArray<WCAddressFrame *> *)mergedFrameArray {
    if (mergedFrameArray == nil || [mergedFrameArray count] == 0) {
        return;
    }
    NSMutableArray<WCAddressFrame *> *notFoundFrame = [NSMutableArray array];
    for (WCAddressFrame *mergedFrame in mergedFrameArray) {
        BOOL bFound = NO;
        for (WCAddressFrame *mainFrame in mainFrameArray) {
            if (mergedFrame.address == mainFrame.address) {
                bFound = YES;
                [self p_mergeAddressFrame:mainFrame with:mergedFrame];
                break;
            }
        }
        if (bFound == NO) {
            [notFoundFrame addObject:mergedFrame];
        }
    }
    [mainFrameArray addObjectsFromArray:notFoundFrame];
}

- (NSString *)description {
    NSMutableString *retStr = [NSMutableString new];

    for (int i = 0; i < [self.parentAddressFrame count]; i++) {
        WCAddressFrame *frame = self.parentAddressFrame[i];
        [retStr appendString:[frame description]];
    }

    return retStr;
}

@end
