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

#import "WCFPSRecorder.h"
#import "WCMatrixModel.h"

#include <map>

struct StackInfo {
    uintptr_t **stack_matrix;
    int *trace_length_matrix;
};

// ============================================================================
#pragma mark - WCAddressFrame
// ============================================================================

#define CHILE_FRAME "child"

// ============================================================================
#pragma mark - WCStackTracePool
// ============================================================================

@interface WCFPSRecorder () {
    uintptr_t **m_stackCyclePool;
    size_t *m_stackCount;
    uint64_t m_poolTailPoint;
    uint32_t m_maxStackCount;
    uint32_t m_repeatCount;
}

/* conlusion */
@property (nonatomic, strong) NSMutableArray<WCAddressFrame *> *parentAddressFrame;

@end

@implementation WCFPSRecorder

- (id)init {
    assert(0);
    return [self initWithMaxStackTraceCount:0 timeInterval:0];
}

- (id)initWithMaxStackTraceCount:(uint32_t)maxStackTraceCount timeInterval:(uint32_t)ti {
    self = [super init];
    if (self) {
        m_maxStackCount = maxStackTraceCount;
        m_repeatCount = ti;
        m_repeatCount = MAX(m_repeatCount, 1);

        size_t cycleArrayBytes = m_maxStackCount * sizeof(uintptr_t *);
        m_stackCyclePool = (uintptr_t **)malloc(cycleArrayBytes);
        if (m_stackCyclePool != NULL) {
            memset(m_stackCyclePool, 0, cycleArrayBytes);
        }
        size_t countArrayBytes = m_maxStackCount * sizeof(size_t);
        m_stackCount = (size_t *)malloc(countArrayBytes);
        if (m_stackCount != NULL) {
            memset(m_stackCount, 0, countArrayBytes);
        }
        m_poolTailPoint = 0;
    }
    return self;
}

- (void)dealloc {
    for (uint32_t i = 0; i < m_maxStackCount; i++) {
        if (m_stackCyclePool[i] != NULL) {
            free(m_stackCyclePool[i]);
            m_stackCyclePool[i] = NULL;
        }
    }
    if (m_stackCyclePool != NULL) {
        free(m_stackCyclePool);
        m_stackCyclePool = NULL;
    }
    if (m_stackCount != NULL) {
        free(m_stackCount);
        m_stackCount = NULL;
    }
}

- (void)addThreadStack:(uintptr_t *)stackArray andLength:(size_t)stackCount {
    if (stackArray == NULL) {
        return;
    }
    if (m_stackCyclePool == NULL || m_stackCount == NULL) {
        return;
    }
    if (stackCount == 0) {
        return;
    }
    if (m_stackCyclePool[m_poolTailPoint] != NULL) {
        free(m_stackCyclePool[m_poolTailPoint]);
    }
    m_stackCyclePool[m_poolTailPoint] = stackArray;
    m_stackCount[m_poolTailPoint] = stackCount;

    m_poolTailPoint = (m_poolTailPoint + 1) % m_maxStackCount;
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

    return addressDictArray;
}

- (NSDictionary *)p_getInfoDictFromAddressFrame:(WCAddressFrame *)addressFrame {
    NSMutableDictionary *currentInfoDict = [NSMutableDictionary dictionaryWithDictionary:[addressFrame getInfoDict]];
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
        [currentInfoDict setObject:childInfoDict forKey:@CHILE_FRAME];
    }
    return currentInfoDict;
}

- (WCAddressFrame *)p_getAddressFrameWithStackTraces:(uintptr_t *)stackTrace length:(size_t)traceLength {
    if (stackTrace == NULL || traceLength == 0) {
        return nil;
    }
    WCAddressFrame *headAddressFrame = nil;
    WCAddressFrame *currentParentFrame = nil;

    for (int i = (int)traceLength - 1; i >= 0; --i) {
        uintptr_t address = stackTrace[i];
        WCAddressFrame *curFrame = [[WCAddressFrame alloc] initWithAddress:address withRepeatCount:m_repeatCount isInBackground:NO];
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
