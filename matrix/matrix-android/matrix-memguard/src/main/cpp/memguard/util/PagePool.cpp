//
// Created by tomystang on 2020/10/16.
//

#include <unistd.h>
#include <sys/mman.h>
#include <Options.h>
#include <cerrno>
#include <cinttypes>
#include "Auxiliary.h"
#include "PagePool.h"
#include "Memory.h"
#include "Mutex.h"
#include "Log.h"
#include "Issue.h"
#include <linux/prctl.h>

using namespace memguard;

#define LOG_TAG "MemGuard.PagePool"

#define SLOT_AREA_TAG "MemGuard.Slot"
#define META_AREA_TAG "MemGuard.Meta"
#define FREE_SLOT_ID_AREA_TAG "MemGuard.FreeSlotID"

enum InternalGuardSide {
    UNINITIALIZED = 0,
    ON_LEFT = 1,
    ON_RIGHT = 2
};

struct alignas(8) PageMeta {
    bool isBorrowed = false;
    void* allocatedStart = nullptr;
    size_t allocatedSize = 0;
    InternalGuardSide guardSide = UNINITIALIZED;
    pid_t borrowThreadId = 0;
    issue::ThreadName borrowThreadName;
    void* stackTraceOnBorrow[pagepool::MAX_RECORDED_STACKFRAME_COUNT];
    size_t onBorrowStackFrameCount = 0;
    pid_t returnThreadId = 0;
    issue::ThreadName returnThreadName;
    void* stackTraceOnReturn[pagepool::MAX_RECORDED_STACKFRAME_COUNT];
    size_t onReturnStackFrameCount = 0;
};

static Mutex sPoolMutex;

static size_t sGuardPageSize = 0;
static size_t sSlotSize = 0;
static void* sSlotAreaStart = nullptr;
static size_t sSlotAreaSize = 0;
static void* sSlotAreaEnd = nullptr;

static PageMeta* sMetaAreaStart = nullptr;

static pagepool::slot_t* sFreeSlots = nullptr;
static size_t sFreeSlotCount = 0;

static size_t sLentPageCount = 0;

bool memguard::pagepool::Prepare() {
    sGuardPageSize = memory::GetPageSize();
    sSlotSize = AlignUpTo(gOpts.maxAllocationSize, memory::GetPageSize());
    sSlotAreaSize = sGuardPageSize + (sSlotSize + sGuardPageSize) * gOpts.maxDetectableAllocationCount;
    sSlotAreaStart = memory::MapMemArea(sSlotAreaSize, SLOT_AREA_TAG);
    if (sSlotAreaStart == nullptr) {
        int errcode = errno;
        LOGE(LOG_TAG, "Fail to mmap data area, error: %s(%d)", strerror(errcode), errcode);
        return false;
    }
    sSlotAreaEnd = (void*) (((uint64_t) sSlotAreaStart) + sSlotAreaSize);

    size_t metaAreaSize = sizeof(PageMeta) * gOpts.maxDetectableAllocationCount;
    sMetaAreaStart = (PageMeta*) memory::MapMemArea(metaAreaSize, META_AREA_TAG);
    if (sMetaAreaStart == nullptr) {
        int errcode = errno;
        LOGE(LOG_TAG, "Fail to mmap meta area, error: %s(%d)", strerror(errcode), errcode);
        return false;
    }
    memory::MarkAreaReadWrite(sMetaAreaStart, metaAreaSize);
    memset(sMetaAreaStart, 0, metaAreaSize);

    size_t freeSlotsSize = sizeof(slot_t) * gOpts.maxDetectableAllocationCount;
    sFreeSlots = (slot_t*) memory::MapMemArea(freeSlotsSize, FREE_SLOT_ID_AREA_TAG);
    if (sFreeSlots == nullptr) {
        int errcode = errno;
        LOGE(LOG_TAG, "Fail to mmap slot id area, error: %s(%d)", strerror(errcode), errcode);
        return false;
    }
    memory::MarkAreaReadWrite(sFreeSlots, freeSlotsSize);
    memset(sFreeSlots, 0, freeSlotsSize);

    return true;
}

int memguard::pagepool::BorrowSlot(size_t size, size_t alignment, GuardSide guard_side) {
    if (size > sSlotSize) {
        return SLOT_NONE;
    }
    if (AlignUpTo(size, alignment) > pagepool::GetSlotSize()) {
        return SLOT_NONE;
    }

    slot_t resultSlotID = SLOT_NONE;
    {
        sPoolMutex.lock();
        ON_SCOPE_EXIT(sPoolMutex.unlock());

        if (sLentPageCount < gOpts.maxDetectableAllocationCount) {
            resultSlotID = sLentPageCount++;
        } else if (sFreeSlotCount > 0) {
            int pickedSlotIndex = (int) (random::GenerateUnsignedInt32() % sFreeSlotCount);
            resultSlotID = sFreeSlots[pickedSlotIndex];
            sFreeSlots[pickedSlotIndex] = sFreeSlots[--sFreeSlotCount];
        }

        if (resultSlotID == SLOT_NONE) {
            return resultSlotID;
        }
    }

    auto meta = sMetaAreaStart + resultSlotID;
    meta->isBorrowed = true;
    meta->allocatedSize = size;

    meta->borrowThreadId = gettid();
    issue::GetSelfThreadName(meta->borrowThreadName);
    meta->onBorrowStackFrameCount = unwind::UnwindStack(
          meta->stackTraceOnBorrow, pagepool::MAX_RECORDED_STACKFRAME_COUNT);

    meta->returnThreadId = 0;
    meta->returnThreadName[0] = '\0';
    meta->onReturnStackFrameCount = 0;

    off_t allocatedStart = 0;
    if (guard_side == ON_RIGHT) {
        uint64_t padding = pagepool::GetSlotSize() - size;
        if (!gOpts.perfectRightSideGuard) {
            padding = AlignDownTo(padding, alignment);
        }
        allocatedStart = padding;
    }
    void* guardedPageStart = GetGuardedPageStart(resultSlotID);
    meta->allocatedStart = (void*) ((uint64_t) guardedPageStart + allocatedStart);

    void* pagedAllocatedStart = (void*) AlignDownTo((uint64_t) meta->allocatedStart, memory::GetPageSize());
    size_t pagedAllocatedSize = AlignUpTo(size, memory::GetPageSize());
    memory::MarkAreaReadWrite(pagedAllocatedStart, pagedAllocatedSize);
    // memset(pagedAllocatedStart, 0xAD, pagedAllocatedSize);

    return resultSlotID;
}

bool memguard::pagepool::IsSlotBorrowed(slot_t slot) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);
    return sMetaAreaStart[slot].isBorrowed;
}

void memguard::pagepool::ReturnSlot(slot_t slot) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);

    auto meta = sMetaAreaStart + slot;
    if (!meta->isBorrowed) {
        LOGE(LOG_TAG, "Double free detected.");
        issue::TriggerIssue(slot, IssueType::DOUBLE_FREE);
    }

    meta->isBorrowed = false;

    meta->returnThreadId = gettid();
    issue::GetSelfThreadName(meta->returnThreadName);
    meta->onReturnStackFrameCount =
          unwind::UnwindStack(meta->stackTraceOnReturn, pagepool::MAX_RECORDED_STACKFRAME_COUNT);

    void* guardedPageStart = GetGuardedPageStart(slot);
    memory::MarkAreaReadWrite(guardedPageStart, GetSlotSize());
    // memset(guardedPageStart, 0xBF, GetSlotSize());
    memory::MarkAreaInaccessible(guardedPageStart, GetSlotSize());

    {
        sPoolMutex.lock();
        ON_SCOPE_EXIT(sPoolMutex.unlock());

        if (sFreeSlotCount >= gOpts.maxDetectableAllocationCount) {
            return;
        }

        sFreeSlots[sFreeSlotCount++] = slot;
    }
}

/*
 *  Slot Area Layout
 *  +-----+------------+-----+------------+-----+
 *  |     |            |     |            |     |
 *  |     |  Guarded   |     |  Guarded   |     |
 *  |Guard|   Pages    |Guard|   Pages    |Guard|
 *  |     |            |     |            |     |
 *  +------------------+------------------+-----+
 *        |            |     ｜           |
 *        +-+ slot 0 +-+     +-+ slot 1 +-+
 *
 */
void* memguard::pagepool::GetGuardedPageStart(slot_t slot) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);
    return (void*) ((uint64_t) (sSlotAreaStart) + sGuardPageSize + (sSlotSize + sGuardPageSize) * slot);
}

void* memguard::pagepool::GetAllocatedAddress(slot_t slot) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);
    return sMetaAreaStart[slot].allocatedStart;
}

size_t memguard::pagepool::GetAllocatedSize(slot_t slot) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);

    sPoolMutex.lock();
    ON_SCOPE_EXIT(sPoolMutex.unlock());

    return sMetaAreaStart[slot].allocatedSize;
}

bool memguard::pagepool::GetSlotGuardSide(slot_t slot, GuardSide* result) {
    if (slot == SLOT_NONE || result == nullptr) {
        return false;
    }
    auto meta = sMetaAreaStart + slot;
    if (!meta->isBorrowed) {
        return false;
    }
    *result = meta->guardSide == InternalGuardSide::ON_LEFT ? GuardSide::ON_LEFT : GuardSide::ON_RIGHT;
    return true;
}

size_t memguard::pagepool::GetSlotSize() {
    return sSlotSize;
}

pagepool::slot_t memguard::pagepool::GetSlotIdOfAddress(const void* addr) {
    if (addr < sSlotAreaStart || addr >= sSlotAreaEnd) {
        return SLOT_NONE;
    }
    void* firstGuardedPageStart = GetGuardedPageStart(0);
    if (addr < firstGuardedPageStart) {
        return 0;
    }
    uint64_t distance = (uint64_t) addr - (uint64_t) firstGuardedPageStart;
    return (slot_t) (distance / (sSlotSize + sGuardPageSize));
}

bool memguard::pagepool::IsAddressInGuardPage(const void* addr) {
    if (addr < sSlotAreaStart || addr >= sSlotAreaEnd) {
        return false;
    }
    size_t addrPageCount = ((uint64_t) addr - (uint64_t) sSlotAreaStart) / memory::GetPageSize();
    size_t maxAllocationPageCount = sSlotSize / memory::GetPageSize();
    return (addrPageCount % (maxAllocationPageCount + 1)) == 0;
}

void memguard::pagepool::GetThreadInfoOnBorrow(slot_t slot, pid_t* tid_out, char** name_out) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);
    auto meta = sMetaAreaStart + slot;
    *tid_out = meta->borrowThreadId;
    *name_out = meta->borrowThreadName;
}

void memguard::pagepool::GetStackTraceOnBorrow(slot_t slot, void** pcs[], size_t* size) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);
    auto meta = sMetaAreaStart + slot;
    *pcs = meta->stackTraceOnBorrow;
    *size = meta->onBorrowStackFrameCount;
}

void memguard::pagepool::GetThreadInfoOnReturn(slot_t slot, pid_t* tid_out, char** name_out) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);
    auto meta = sMetaAreaStart + slot;
    *tid_out = meta->returnThreadId;
    *name_out = meta->returnThreadName;
}

void memguard::pagepool::GetStackTraceOnReturn(memguard::pagepool::slot_t slot, void** pcs[], size_t* size) {
    ASSERT(slot >= 0 && (unsigned) slot < gOpts.maxDetectableAllocationCount, "Bad slot: %" PRId64, slot);
    auto meta = sMetaAreaStart + slot;
    *pcs = meta->stackTraceOnReturn;
    *size = meta->onReturnStackFrameCount;
}