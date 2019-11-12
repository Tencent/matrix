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
#import <pthread.h>
#import "MatrixLogDef.h"

#define STACK_PER_MAX_COUNT 100 // the max address count of one stack

#define SHORTEST_LENGTH_OF_STACK 10

@interface WCMainThreadHandler () {
    pthread_mutex_t m_threadLock;
    int m_cycleArrayCount;
    
    uintptr_t **m_mainThreadStackCycleArray;
    size_t *m_mainThreadStackCount;
    uint64_t m_tailPoint;
    size_t *m_topStackAddressRepeatArray;
    int *m_mainThreadStackRepeatCountArray;
}

@end

@implementation WCMainThreadHandler

- (id)initWithCycleArrayCount:(int)cycleArrayCount
{
    self = [super init];
    if (self) {
        m_cycleArrayCount = cycleArrayCount;

        size_t cycleArrayBytes = m_cycleArrayCount * sizeof(uintptr_t *);
        m_mainThreadStackCycleArray = (uintptr_t **) malloc(cycleArrayBytes);
        if (m_mainThreadStackCycleArray != NULL) {
            memset(m_mainThreadStackCycleArray, 0, cycleArrayBytes);
        }
        size_t countArrayBytes = m_cycleArrayCount * sizeof(size_t);
        m_mainThreadStackCount = (size_t *) malloc(countArrayBytes);
        if (m_mainThreadStackCount != NULL) {
            memset(m_mainThreadStackCount, 0, countArrayBytes);
        }
        size_t addressArrayBytes = m_cycleArrayCount * sizeof(size_t);
        m_topStackAddressRepeatArray = (size_t *) malloc(addressArrayBytes);
        if (m_topStackAddressRepeatArray != NULL) {
            memset(m_topStackAddressRepeatArray, 0, addressArrayBytes);
        }

        m_tailPoint = 0;

        pthread_mutex_init(&m_threadLock, NULL);
    }
    return self;
}

- (id)init
{
    return [self initWithCycleArrayCount:10];
}

- (void)dealloc
{
    for (uint32_t i = 0; i < m_cycleArrayCount; i++) {
        if (m_mainThreadStackCycleArray[i] != NULL) {
            free(m_mainThreadStackCycleArray[i]);
            m_mainThreadStackCycleArray[i] = NULL;
        }
    }
    
    if (m_mainThreadStackCycleArray != NULL) {
        free(m_mainThreadStackCycleArray);
        m_mainThreadStackCycleArray = NULL;
    }
    
    if (m_mainThreadStackCount != NULL) {
        free(m_mainThreadStackCount);
        m_mainThreadStackCount = NULL;
    }
    
    if (m_topStackAddressRepeatArray != NULL) {
        free(m_topStackAddressRepeatArray);
        m_topStackAddressRepeatArray = NULL;
    }
    
    if (m_mainThreadStackRepeatCountArray != NULL) {
        free(m_mainThreadStackRepeatCountArray);
        m_mainThreadStackRepeatCountArray = NULL;
    }
}

- (void)addThreadStack:(uintptr_t *)stackArray andStackCount:(size_t)stackCount
{
    if (stackArray == NULL) {
        return;
    }

    if (m_mainThreadStackCycleArray == NULL || m_mainThreadStackCount == NULL) {
        return;
    }

    pthread_mutex_lock(&m_threadLock);
  
    if (m_mainThreadStackCycleArray[m_tailPoint] != NULL) {
        free(m_mainThreadStackCycleArray[m_tailPoint]);
    }
    m_mainThreadStackCycleArray[m_tailPoint] = stackArray;
    m_mainThreadStackCount[m_tailPoint] = stackCount;

    uint64_t lastTailPoint = (m_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
    uintptr_t lastTopStack = 0;
    if (m_mainThreadStackCycleArray[lastTailPoint] != NULL) {
        lastTopStack = m_mainThreadStackCycleArray[lastTailPoint][0];
    }
    uintptr_t currentTopStackAddr = stackArray[0];
    if (lastTopStack == currentTopStackAddr) {
        size_t lastRepeatCount = m_topStackAddressRepeatArray[lastTailPoint];
        m_topStackAddressRepeatArray[m_tailPoint] = lastRepeatCount + 1;
    } else {
        m_topStackAddressRepeatArray[m_tailPoint] = 0;
    }

    m_tailPoint = (m_tailPoint + 1) % m_cycleArrayCount;
    pthread_mutex_unlock(&m_threadLock);
}

- (size_t)getLastMainThreadStackCount
{
    uint64_t lastPoint = (m_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
    return m_mainThreadStackCount[lastPoint];
}

- (uintptr_t *)getLastMainThreadStack
{
    uint64_t lastPoint = (m_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
    return m_mainThreadStackCycleArray[lastPoint];
}

- (KSStackCursor *)getPointStackCursor
{
    pthread_mutex_lock(&m_threadLock);
    size_t maxValue = 0;
    BOOL trueStack = NO;
    for (int i = 0; i < m_cycleArrayCount; i++) {
        size_t currentValue = m_topStackAddressRepeatArray[i];
        int stackCount = (int) m_mainThreadStackCount[i];
        if (currentValue >= maxValue && stackCount > SHORTEST_LENGTH_OF_STACK) {
            maxValue = currentValue;
            trueStack = YES;
        }
    }
    
    if (!trueStack) {
        return NULL;
    }

    size_t currentIndex = (m_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
    for (int i = 0; i < m_cycleArrayCount; i++) {
        int trueIndex = (m_tailPoint + m_cycleArrayCount - i - 1) % m_cycleArrayCount;
        int stackCount = (int) m_mainThreadStackCount[trueIndex];
        if (m_topStackAddressRepeatArray[trueIndex] == maxValue && stackCount > SHORTEST_LENGTH_OF_STACK) {
            currentIndex = trueIndex;
            break;
        }
    }

    // current count of point stack
    size_t stackCount = m_mainThreadStackCount[currentIndex];
    size_t pointThreadSize = sizeof(uintptr_t) * stackCount;
    uintptr_t *pointThreadStack = (uintptr_t *) malloc(pointThreadSize);
    
    size_t repeatCountArrayBytes = stackCount * sizeof(int);
    m_mainThreadStackRepeatCountArray = (int *) malloc(repeatCountArrayBytes);
    if (m_mainThreadStackRepeatCountArray != NULL) {
        memset(m_mainThreadStackRepeatCountArray, 0, repeatCountArrayBytes);
    }
    
    // calculate the repeat count
    for (size_t i = 0; i < stackCount; i++) {
        for (int innerIndex = 0; innerIndex < m_cycleArrayCount; innerIndex++) {
            size_t innerStackCount = m_mainThreadStackCount[innerIndex];
            for (size_t idx = 0; idx < innerStackCount; idx++) {
                
                // point stack i_th address compare to others
                if (m_mainThreadStackCycleArray[currentIndex][i] == m_mainThreadStackCycleArray[innerIndex][idx]) {
                    m_mainThreadStackRepeatCountArray[i] += 1;
                }
            }
        }
    }
    
    if (pointThreadStack != NULL) {
        memset(pointThreadStack, 0, pointThreadSize);
        for (size_t idx = 0; idx < stackCount; idx++) {
            pointThreadStack[idx] = m_mainThreadStackCycleArray[currentIndex][idx];
        }
        KSStackCursor *pointCursor = (KSStackCursor *) malloc(sizeof(KSStackCursor));
        kssc_initWithBacktrace(pointCursor, pointThreadStack, (int) stackCount, 0);
        pthread_mutex_unlock(&m_threadLock);
        return pointCursor;
    }
    pthread_mutex_unlock(&m_threadLock);
    return NULL;
    /* Old
     OSSpinLockLock(&m_threadLock);
     uint64_t lastPoint = (m_tailPoint + m_cycleArrayCount - 1) % m_cycleArrayCount;
     uintptr_t *lastStack = m_mainThreadStackCycleArray[lastPoint];
     uint64_t lastStackCount = m_mainThreadStackCount[lastPoint];
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

- (int *)getPointStackRepeatCount
{
    return m_mainThreadStackRepeatCountArray;
}

- (KSStackCursor **)getStackCursorWithLimit:(int)limitCount withReturnSize:(NSUInteger &)stackSize
{
    KSStackCursor **allStackCursor = (KSStackCursor **) malloc(sizeof(KSStackCursor *) * limitCount);
    if (allStackCursor == NULL) {
        //        BMDebug(@"allStackCursor == NULL");
        return NULL;
    }
    stackSize = 0;
    pthread_mutex_lock(&m_threadLock);
    for (int i = 0; i < limitCount; i++) {
        uint64_t trueIndex = (m_tailPoint + m_cycleArrayCount - i - 1) % m_cycleArrayCount;
        if (m_mainThreadStackCycleArray[trueIndex] == NULL) {
            MatrixDebug(@"m_mainThreadStackCycleArray == NULL");
            break;
        }
        size_t currentStackCount = m_mainThreadStackCount[trueIndex];
        uintptr_t *currentThreadStack = (uintptr_t *) malloc(sizeof(uintptr_t) * currentStackCount);
        if (currentThreadStack == NULL) {
            MatrixDebug(@"currentThreadStack == NULL");
            break;
        }
        stackSize++;
        for (int j = 0; j < currentStackCount; j++) {
            currentThreadStack[j] = m_mainThreadStackCycleArray[trueIndex][j];
        }
        KSStackCursor *currentStackCursor = (KSStackCursor *) malloc(sizeof(KSStackCursor));
        kssc_initWithBacktrace(currentStackCursor, currentThreadStack, (int) currentStackCount, 0);
        allStackCursor[i] = currentStackCursor;
    }
    pthread_mutex_unlock(&m_threadLock);
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

    pthread_mutex_lock(&m_threadLock);
    for (int i = 0; i < m_cycleArrayCount; i++) {
        uint64_t trueIndex = (m_tailPoint + m_cycleArrayCount - i - 1) % m_cycleArrayCount;
        if (m_mainThreadStackCycleArray[trueIndex] == NULL) {
            MatrixDebug(@"m_mainThreadStackCycleArray == NULL");
            break;
        }
        size_t currentStackCount = m_mainThreadStackCount[trueIndex];
        uintptr_t *currentThreadStack = (uintptr_t *) malloc(sizeof(uintptr_t) * currentStackCount);
        if (currentThreadStack == NULL) {
            MatrixDebug(@"currentThreadStack == NULL");
            break;
        }
        stackSize++;
        for (int j = 0; j < currentStackCount; j++) {
            currentThreadStack[j] = m_mainThreadStackCycleArray[trueIndex][j];
        }
        KSStackCursor *currentStackCursor = (KSStackCursor *) malloc(sizeof(KSStackCursor));
        kssc_initWithBacktrace(currentStackCursor, currentThreadStack, (int) currentStackCount, 0);
        allStackCursor[i] = currentStackCursor;
    }
    pthread_mutex_unlock(&m_threadLock);
    return allStackCursor;
}

- (uint32_t)p_findRepeatCountInArrayWithAddr:(uintptr_t)addr
{
    uint32_t repeatCount = 0;
    for (size_t idx = 0; idx < m_cycleArrayCount; idx++) {
        uintptr_t *stack = m_mainThreadStackCycleArray[idx];
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

