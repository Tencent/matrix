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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "KSSnapshot.h"
#include "KSStackCursor_Backtrace.h"
#include "KSStackCursor_MachineContext.h"

#define CALLSTACK_MAX_DEPTH 100
#define CALLSTACK_AVG_DEPTH 50

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define NULL_GUARD(value)  \
    if ((value) == NULL) { \
        assert(false);     \
        return;            \
    }
#define NULL_GUARD_RETURN(value)           \
    if ((value) == NULL) {                 \
        assert(false);                     \
        return KSSnapshotInvalidParameter; \
    }

KSSnapshotStatus kssnapshot_initWithMachineContext(KSSnapshot *snapshot, const KSMachineContext *machineContext) {
    NULL_GUARD_RETURN(snapshot)
    NULL_GUARD_RETURN(machineContext)

    int threadCount = ksmc_getThreadCount(machineContext);
    if (threadCount <= 0) {
        return KSSnapshotInvalidParameter;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    snapshot->machineContext = machineContext;
    snapshot->threadCount = threadCount;
    snapshot->addressArray = (uintptr_t *)calloc(threadCount * CALLSTACK_AVG_DEPTH, sizeof(uintptr_t));
    snapshot->startPointArray = (uintptr_t **)calloc(threadCount, sizeof(uintptr_t *));
    snapshot->stackDepthArray = (int *)calloc(threadCount, sizeof(int));
    snapshot->machineContextArray = (KSMachineContextLite *)calloc(threadCount, sizeof(KSMachineContextLite));

    if (snapshot->addressArray == NULL
        || snapshot->startPointArray == NULL
        || snapshot->stackDepthArray == NULL
        || snapshot->machineContextArray == NULL) {
        return KSSnapshotAllocationFailed;
    }

    return KSSnapshotSucceeded;
}

void kssnapshot_deinit(KSSnapshot *snapshot) {
    NULL_GUARD(snapshot)

    if (snapshot->addressArray != NULL) {
        free(snapshot->addressArray);
    }

    if (snapshot->startPointArray != NULL) {
        free(snapshot->startPointArray);
    }

    if (snapshot->stackDepthArray != NULL) {
        free(snapshot->stackDepthArray);
    }

    if (snapshot->machineContextArray != NULL) {
        free(snapshot->machineContextArray);
    }

    memset(snapshot, 0, sizeof(*snapshot));
}

void kssnapshot_convertMachineContextFullToLite(const KSMachineContext *machineContext, KSMachineContextLite *machineContextLite) {
    NULL_GUARD(machineContext)
    NULL_GUARD(machineContextLite)

    memset(machineContextLite, 0, sizeof(*machineContextLite));

    machineContextLite->thisThread = machineContext->thisThread;
    machineContextLite->isCrashedContext = machineContext->isCrashedContext;
    machineContextLite->isCurrentThread = machineContext->isCurrentThread;
    machineContextLite->isSignalContext = machineContext->isSignalContext;
    machineContextLite->cpuContext.__es = machineContext->machineContext.__es;
    machineContextLite->cpuContext.__ss = machineContext->machineContext.__ss;
}

void kssnapshot_convertMachineContextLiteToFull(const KSMachineContextLite *machineContextLite, KSMachineContext *machineContext) {
    NULL_GUARD(machineContext)
    NULL_GUARD(machineContextLite)

    memset(machineContext, 0, sizeof(*machineContext));

    machineContext->thisThread = machineContextLite->thisThread;
    machineContext->isCrashedContext = machineContextLite->isCrashedContext;
    machineContext->isCurrentThread = machineContextLite->isCurrentThread;
    machineContext->isSignalContext = machineContextLite->isSignalContext;
    machineContext->machineContext.__es = machineContextLite->cpuContext.__es;
    machineContext->machineContext.__ss = machineContextLite->cpuContext.__ss;
}

KSSnapshotStatus kssnapshot_takeSnapshot(KSSnapshot *snapshot) {
    NULL_GUARD_RETURN(snapshot)

    KSMC_NEW_CONTEXT(localContext);
    KSStackCursor stackCursor;

    const KSMachineContext *globalContext = snapshot->machineContext;
    KSThread currentThread = ksmc_getThreadFromContext(globalContext);
    int threadCount = ksmc_getThreadCount(globalContext);

    if (threadCount != snapshot->threadCount) {
        assert(false);
        return KSSnapshotInvalidParameter;
    }

    uintptr_t *startPointer = snapshot->addressArray;

    for (int i = 0; i < threadCount; i++) {
        KSThread thread = ksmc_getThreadAtIndex(globalContext, i);
        if (thread == currentThread) {
            // skip current thread
        } else {
            size_t used = startPointer - snapshot->addressArray;
            size_t remained = threadCount * CALLSTACK_AVG_DEPTH - used;
            if (remained <= 0) {
                return KSSnapshotBufferFull;
            }

            ksmc_getContextForThread(thread, localContext, false);
            kssc_initWithMachineContext(&stackCursor, (int)MIN(CALLSTACK_MAX_DEPTH, remained), localContext);

            snapshot->startPointArray[i] = startPointer;

            int depth = 0;
            while (stackCursor.advanceCursor(&stackCursor)) {
                *startPointer = stackCursor.stackEntry.address;
                startPointer++;
                depth++;
            }

            snapshot->stackDepthArray[i] = depth;

            kssnapshot_convertMachineContextFullToLite(localContext, &snapshot->machineContextArray[i]);
        }
    }

    return KSSnapshotSucceeded;
}

KSSnapshotStatus kssnapshot_restore(const KSSnapshot *snapshot, int index, KSMachineContext *machineContext, KSStackCursor *stackCursor) {
    NULL_GUARD_RETURN(snapshot)
    NULL_GUARD_RETURN(machineContext)
    NULL_GUARD_RETURN(stackCursor)

    if (index >= snapshot->threadCount) {
        assert(false);
        return KSSnapshotInvalidParameter;
    }

    const KSMachineContextLite *machineContextLite = &snapshot->machineContextArray[index];
    kssnapshot_convertMachineContextLiteToFull(machineContextLite, machineContext);

    uintptr_t *callstack = snapshot->startPointArray[index];
    int stackDepth = snapshot->stackDepthArray[index];
    kssc_initWithBacktrace(stackCursor, callstack, stackDepth, 0);

    return KSSnapshotSucceeded;
}
