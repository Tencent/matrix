//
// Created by tomystang on 2020/10/16.
//

#include <pthread.h>
#include "PagePool.h"
#include "Auxiliary.h"
#include "Allocation.h"
#include "Options.h"

using namespace memguard;

static size_t sAdjustedPercentageOfLeftSideGuard = 0;
static size_t sSampleCycleLength = 0;

struct alignas(8) TLSValues {
    uint32_t sampleCounter = 0;
    bool isCallingAlloc = false;
    bool isCallingFree = false;
};

static TLSValues TLSVAR sTLSValues;

static bool SUPRESS_UNUSED ShouldBeGuardedThisTime() {
    if (UNLIKELY(sTLSValues.sampleCounter == 0)) {
        sTLSValues.sampleCounter = (random::GenerateUnsignedInt32() & (sSampleCycleLength - 1));
        return true;
    } else {
        --sTLSValues.sampleCounter;
        return false;
    }
}

static bool SUPRESS_UNUSED ShouldBeGuardedOnLeft() {
    //      rnd % 1024 <= percentage / 100 * 1024
    //  ==> rnd % 1024 * 100 <= percentage * 1024
    return (memguard::random::GenerateUnsignedInt32() & ((1U << 10U) - 1)) * 100U
        <= (sAdjustedPercentageOfLeftSideGuard << 10U);
}

#define RECURSIVE_GUARD(tls_var, ret_value_on_reentered) \
    if (tls_var) { \
      return (ret_value_on_reentered); \
    } \
    (tls_var) = true; \
    ON_SCOPE_EXIT((tls_var) = false)


bool memguard::allocation::Prepare() {
    sAdjustedPercentageOfLeftSideGuard = gOpts.percentageOfLeftSideGuard;
    if (sAdjustedPercentageOfLeftSideGuard > 100) {
        sAdjustedPercentageOfLeftSideGuard = 100;
    }

    sSampleCycleLength = AlignUpToPowOf2(gOpts.maxSkippedAllocationCount);
    if (sSampleCycleLength == 0) {
        sSampleCycleLength = 1;
    } else if (sSampleCycleLength == 1) {
        sSampleCycleLength = 2;
    }

    random::InitializeRndSeed();

    sTLSValues.sampleCounter = (random::GenerateUnsignedInt32() & (sSampleCycleLength - 1));

    return true;
}

void* memguard::allocation::Allocate(size_t size) {
    return AlignedAllocate(size, (sizeof(void*) << 1));
}

void* memguard::allocation::AlignedAllocate(size_t size, size_t alignment) {
    RECURSIVE_GUARD(sTLSValues.isCallingAlloc, nullptr);

    if (!ShouldBeGuardedThisTime()) {
        return nullptr;
    }

    if (UNLIKELY(alignment > pagepool::GetSlotSize())) {
        return nullptr;
    }

    if (UNLIKELY(!IsPowOf2(alignment))) {
        return nullptr;
    }

    if (UNLIKELY(AlignUpTo(size, alignment) > pagepool::GetSlotSize())) {
        return nullptr;
    }

    pagepool::GuardSide guardSide = ShouldBeGuardedOnLeft()
          ? pagepool::GuardSide::ON_LEFT
          : pagepool::GuardSide::ON_RIGHT;
    int slot = pagepool::BorrowSlot(size, alignment, guardSide);
    if (slot == pagepool::SLOT_NONE) {
        return nullptr;
    }

    return pagepool::GetAllocatedAddress(slot);
}

bool memguard::allocation::IsAllocatedByThisAllocator(void* ptr) {
    return pagepool::GetSlotIdOfAddress(ptr) != pagepool::SLOT_NONE;
}

size_t memguard::allocation::GetAllocatedSize(void* ptr) {
    int slot = pagepool::GetSlotIdOfAddress(ptr);
    if (slot == pagepool::SLOT_NONE) {
        return 0;
    }
    return pagepool::GetAllocatedSize(slot);
}

bool memguard::allocation::Free(void* ptr) {
    RECURSIVE_GUARD(sTLSValues.isCallingFree, false);

    int slot = pagepool::GetSlotIdOfAddress(ptr);
    if (slot != pagepool::SLOT_NONE) {
        pagepool::ReturnSlot(slot);
        return true;
    } else {
        return false;
    }
}