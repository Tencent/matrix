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

#ifndef KSSnapshot_h
#define KSSnapshot_h

#ifdef __cplusplus
extern "C" {
#endif

#include <KSMachineContext_Apple.h>
#include <KSStackCursor.h>

// a lite version of STRUCT_MCONTEXT_L in KSMachineContext
#ifdef __arm64__
typedef struct {
    _STRUCT_ARM_EXCEPTION_STATE64   __es;
    _STRUCT_ARM_THREAD_STATE64      __ss;
} KSCpuContext;
#elif __arm__
typedef struct {
    _STRUCT_ARM_EXCEPTION_STATE     __es;
    _STRUCT_ARM_THREAD_STATE        __ss;
} KSCpuContext;
#elif __x86_64__
typedef struct {
    _STRUCT_X86_EXCEPTION_STATE64   __es;
    _STRUCT_X86_THREAD_STATE64      __ss;
} KSCpuContext;
#endif

// a lite version of KSMachineContext
typedef struct {
    thread_t thisThread;
    bool isCrashedContext;
    bool isCurrentThread;
    bool isSignalContext;
    KSCpuContext cpuContext;
} KSMachineContextLite;

typedef struct {
    const KSMachineContext *machineContext;
    int threadCount;
    uintptr_t *addressArray;
    uintptr_t **startPointArray;
    int *stackDepthArray;
    KSMachineContextLite *machineContextArray;
} KSSnapshot;

typedef enum {
    KSSnapshotTurnedOff = 0,
    KSSnapshotSucceeded = 1,
    KSSnapshotInvalidParameter = -1,
    KSSnapshotAllocationFailed = -2,
    KSSnapshotBufferFull = -3,
} KSSnapshotStatus;

// unsafe to use when other threads are suspended
KSSnapshotStatus kssnapshot_initWithMachineContext(KSSnapshot *snapshot, const KSMachineContext *machineContext);
void kssnapshot_deinit(KSSnapshot *snapshot);

// safe to use when other threads are suspended
KSSnapshotStatus kssnapshot_takeSnapshot(KSSnapshot *snapshot);
KSSnapshotStatus kssnapshot_restore(const KSSnapshot *snapshot, int index, KSMachineContext *machineContext, KSStackCursor *stackCursor);

#ifdef __cplusplus
}
#endif

#endif /* KSSnapshot_h */
