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

#import "WCMainThreadHandler.h"
#import <libkern/OSAtomic.h>
#import "MatrixLogDef.h"

#define STACK_PER_MAX_COUNT 100 // the max address count of one stack

static uintptr_t **g_mainThreadStackCycleArray;
static size_t *g_mainThreadStackCount;
static uint64_t g_tailPoint;
static size_t *g_topStackAddressRepeatArray;

@interface WCMainThreadHandler () {
    OSSpinLock m_threadLock;
    int m_cycleArrayCount;
}

@end

@implementation WCMainThreadHandler

- (id)initWithCycleArrayCount:(int)cycleArrayCount
{
    self = [super init];
    if (self) {
        m_cycleArrayCount = cycleArrayCount;

        size_t cycleArrayBytes = m_cycleArrayCount * sizeof(uintptr_t *);
        g_mainThreadStackCycleArray = (uintptr_t **) malloc(cycleArrayBytes);
        if (g_mainThreadStackCycleArray != NULL) {
            memset(g_mainThreadStackCycleArray, 0, cycleArrayBytes);
        }
        size_t countArrayBytes = m_cycleArrayCount * sizeof(size_t);
        g_mainThreadStackCount = (size_t *) malloc(countArrayBytes);
        if (g_mainThreadStackCount != NULL) {
            memset(g_mainThreadStackCount, 0, countArrayBytes);
        }
        size_t addressArrayBytes = m_cycleArrayCount * sizeof(size_t);
        g_topStackAddressRepeatArray = (size_t *) malloc(addressArrayBytes);
        if (g_topStackAddressRepeatArray != NULL) {
            memset(g_topStackAddressRepeatArray, 0, addressArrayBytes);
        }

        g_tailPoint = 0;

        m_threadLock = OS_SPINLOCK_INIT;
    }
    return self;
}

- (id)init
{
    return [self initWithCycleArrayCount:10];
}

- (void)freeMainThreadCycleArray
{
    for (uint32_t i = 0; i < m_cycleArrayCount; i++) {
        if (g_mainThreadStackCycleArray[i] != NULL) {
            free(g_mainThreadStackCycleArray[i]);
            g_mainThreadStackCycleArray[i] = NULL;
        }
    }
    free(g_mainThreadStackCycleArray);
    g_mainThreadStackCycleArray = NULL;
}
- (void)dealloc
{
    [self freeMainThreadCycleArray];
    free(g_mainThreadStackCount);
}

- (void)addThreadStack:(uintptr_t *)stackArray andStackCount:(size_t)stackCount
{
    if (stackArray == NULL) {
        return;
    }

    if (g_mainThreadStackCycleArray == NULL || g_mainThreadStackCount == NULL) {
        return;
    }

    OSSpinLockLock(&m_threadLock);
    if (g_mainThreadStackCycleArray[g_tailPoint] != NULL) {
        free(g_mainThreadStackCycleArray[g_tailPoint]);
    }
    g_mainThreadStackCycleArray[g_tailPoint] = stackArray;
    g_mainThreadStackCount[g_tailPoint] = stackCount;

    uint64_t lastTailPoint = (g_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
    uintptr_t lastTopStack = 0;
    if (g_mainThreadStackCycleArray[lastTailPoint] != NULL) {
        lastTopStack = g_mainThreadStackCycleArray[lastTailPoint][0];
    }
    uintptr_t currentTopStackAddr = stackArray[0];
    if (lastTopStack == currentTopStackAddr) {
        size_t lastRepeatCount = g_topStackAddressRepeatArray[lastTailPoint];
        g_topStackAddressRepeatArray[g_tailPoint] = lastRepeatCount + 1;
    } else {
        g_topStackAddressRepeatArray[g_tailPoint] = 0;
    }

    g_tailPoint = (g_tailPoint + 1) % m_cycleArrayCount;
    OSSpinLockUnlock(&m_threadLock);
}

- (size_t)getLastMainThreadStackCount
{
    uint64_t lastPoint = (g_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
    return g_mainThreadStackCount[lastPoint];
}

- (uintptr_t *)getLastMainThreadStack
{
    uint64_t lastPoint = (g_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
    return g_mainThreadStackCycleArray[lastPoint];
}

- (KSStackCursor *)getPointStackCursor
{
    OSSpinLockLock(&m_threadLock);
    size_t maxValue = 0;
    for (int i = 0; i < m_cycleArrayCount; i++) {
        size_t currentValue = g_topStackAddressRepeatArray[i];
        if (currentValue > maxValue) {
            maxValue = currentValue;
        }
    }

    size_t currentIndex = (g_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
    for (int i = 0; i < m_cycleArrayCount; i++) {
        int trueIndex = (g_tailPoint + m_cycleArrayCount - i - 1) % m_cycleArrayCount;
        if (g_topStackAddressRepeatArray[trueIndex] == maxValue) {
            currentIndex = trueIndex;
            break;
        }
    }

    size_t stackCount = g_mainThreadStackCount[currentIndex];
    size_t pointThreadSize = sizeof(uintptr_t) * stackCount;
    uintptr_t *pointThreadStack = (uintptr_t *) malloc(pointThreadSize);
    if (pointThreadStack != NULL) {
        memset(pointThreadStack, 0, pointThreadSize);
        for (size_t idx = 0; idx < stackCount; idx++) {
            pointThreadStack[idx] = g_mainThreadStackCycleArray[currentIndex][idx];
        }
        KSStackCursor *pointCursor = (KSStackCursor *) malloc(sizeof(KSStackCursor));
        kssc_initWithBacktrace(pointCursor, pointThreadStack, (int) stackCount, 0);
        OSSpinLockUnlock(&m_threadLock);
        return pointCursor;
    }
    OSSpinLockUnlock(&m_threadLock);
    return NULL;
    /* Old
     OSSpinLockLock(&m_threadLock);
     uint64_t lastPoint = (g_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
     uintptr_t *lastStack = g_mainThreadStackCycleArray[lastPoint];
     uint64_t lastStackCount = g_mainThreadStackCount[lastPoint];
     if (lastStack == NULL || lastStackCount == 0) {
     return NULL;
     }
     
     uint32_t maxRepeatCount = 0;
     size_t maxIdx = 0;
     for (size_t idx = 0; idx < lastStackCount; idx++) {
     uintptr_t addr = lastStack[idx];
     uint32_t currentRepeat = [self p_findRepeatCountInArrayWithAddr:addr];
     if (currentRepeat > maxRepeatCount) {
     maxIdx = idx;
     maxRepeatCount = currentRepeat;
     }
     }
     
     size_t pointThreadCount = lastStackCount - maxIdx;
     uintptr_t *pointThreadStack = (uintptr_t *)malloc(sizeof(uintptr_t) * pointThreadCount);
     size_t filterCount = lastStackCount - pointThreadCount;
     if (pointThreadStack != NULL) {
     memset(pointThreadStack, 0, pointThreadCount);
     for (size_t idx = 0; idx < pointThreadCount; idx++) {
     pointThreadStack[idx] = lastStack[idx + filterCount];
     }
     KSStackCursor *pointCursor = (KSStackCursor *)malloc(sizeof(KSStackCursor));
     kssc_initWithBacktrace(pointCursor, pointThreadStack, (int)pointThreadCount, 0);
     OSSpinLockUnlock(&m_threadLock);
     return pointCursor;
     }
     OSSpinLockUnlock(&m_threadLock);
     return NULL;*/
}

- (KSStackCursor **)getStackCursorWithLimit:(int)limitCount withReturnSize:(NSUInteger &)stackSize
{
    KSStackCursor **allStackCursor = (KSStackCursor **) malloc(sizeof(KSStackCursor *) * limitCount);
    if (allStackCursor == NULL) {
        //        BMDebug(@"allStackCursor == NULL");
        return NULL;
    }
    stackSize = 0;
    OSSpinLockLock(&m_threadLock);
    for (int i = 0; i < limitCount; i++) {
        uint64_t trueIndex = (g_tailPoint + m_cycleArrayCount - i - 1) % m_cycleArrayCount;
        if (g_mainThreadStackCycleArray[trueIndex] == NULL) {
            MatrixDebug(@"g_mainThreadStackCycleArray == NULL");
            break;
        }
        size_t currentStackCount = g_mainThreadStackCount[trueIndex];
        uintptr_t *currentThreadStack = (uintptr_t *) malloc(sizeof(uintptr_t) * currentStackCount);
        if (currentThreadStack == NULL) {
            MatrixDebug(@"currentThreadStack == NULL");
            break;
        }
        stackSize++;
        for (int j = 0; j < currentStackCount; j++) {
            currentThreadStack[j] = g_mainThreadStackCycleArray[trueIndex][j];
        }
        KSStackCursor *currentStackCursor = (KSStackCursor *) malloc(sizeof(KSStackCursor));
        kssc_initWithBacktrace(currentStackCursor, currentThreadStack, (int) currentStackCount, 0);
        allStackCursor[i] = currentStackCursor;
    }
    OSSpinLockUnlock(&m_threadLock);
    return allStackCursor;
}

- (KSStackCursor **)getAllStackCursorWithReturnSize:(NSUInteger &)stackSize
{
    KSStackCursor **allStackCursor = (KSStackCursor **) malloc(sizeof(KSStackCursor *) * m_cycleArrayCount);
    if (allStackCursor == NULL) {
        //        BMDebug(@"allStackCursor == NULL");
        return NULL;
    }
    stackSize = 0;
    OSSpinLockLock(&m_threadLock);
    for (int i = 0; i < m_cycleArrayCount; i++) {
        uint64_t trueIndex = (g_tailPoint + m_cycleArrayCount - i - 1) % m_cycleArrayCount;
        if (g_mainThreadStackCycleArray[trueIndex] == NULL) {
            MatrixDebug(@"g_mainThreadStackCycleArray == NULL");
            break;
        }
        size_t currentStackCount = g_mainThreadStackCount[trueIndex];
        uintptr_t *currentThreadStack = (uintptr_t *) malloc(sizeof(uintptr_t) * currentStackCount);
        if (currentThreadStack == NULL) {
            MatrixDebug(@"currentThreadStack == NULL");
            break;
        }
        stackSize++;
        for (int j = 0; j < currentStackCount; j++) {
            currentThreadStack[j] = g_mainThreadStackCycleArray[trueIndex][j];
        }
        KSStackCursor *currentStackCursor = (KSStackCursor *) malloc(sizeof(KSStackCursor));
        kssc_initWithBacktrace(currentStackCursor, currentThreadStack, (int) currentStackCount, 0);
        allStackCursor[i] = currentStackCursor;
    }
    OSSpinLockUnlock(&m_threadLock);
    return allStackCursor;
}

- (uint32_t)p_findRepeatCountInArrayWithAddr:(uintptr_t)addr
{
    uint32_t repeatCount = 0;
    for (size_t idx = 0; idx < m_cycleArrayCount; idx++) {
        uintptr_t *stack = g_mainThreadStackCycleArray[idx];
        repeatCount += [WCMainThreadHandler p_findRepeatCountInStack:stack withAddr:addr];
    }
    return repeatCount;
}

+ (uint32_t)p_findRepeatCountInStack:(uintptr_t *)stack withAddr:(uintptr_t)addr
{
    if (stack == NULL) {
        return 0;
    }
    uint32_t repeatCount = 0;
    size_t idx = 0;
    while (stack[idx] != 0 || idx < STACK_PER_MAX_COUNT) {
        if (stack[idx] == addr) {
            repeatCount++;
        }
        idx++;
    }
    return repeatCount;
}

// ============================================================================
#pragma mark - Setting
// ============================================================================

- (size_t)getStackMaxCount
{
    return STACK_PER_MAX_COUNT;
}

@end

