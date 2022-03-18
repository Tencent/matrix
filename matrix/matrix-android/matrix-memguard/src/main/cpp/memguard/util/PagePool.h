//
// Created by tomystang on 2020/10/16.
//

#ifndef __MEMGUARD_PAGEPOOL_H__
#define __MEMGUARD_PAGEPOOL_H__


#include <cstdlib>

namespace memguard {
    namespace pagepool {
        static constexpr const size_t MAX_RECORDED_STACKFRAME_COUNT = 16;

        typedef int64_t slot_t;

        static constexpr const slot_t SLOT_NONE = -1;

        enum GuardSide {
            ON_LEFT = 1,
            ON_RIGHT = 2
        };

        extern bool Prepare();

        extern int BorrowSlot(size_t size, size_t alignment, GuardSide guard_side);
        extern bool IsSlotBorrowed(slot_t slot);
        extern void ReturnSlot(slot_t slot);

        extern void* GetGuardedPageStart(slot_t slot);
        extern void* GetAllocatedAddress(slot_t slot);
        extern size_t GetAllocatedSize(slot_t slot);
        extern bool GetSlotGuardSide(slot_t slot, GuardSide* result);
        extern size_t GetSlotSize();

        extern slot_t GetSlotIdOfAddress(const void* addr);
        extern bool IsAddressInGuardPage(const void* addr);

        extern void GetThreadInfoOnBorrow(slot_t slot, pid_t* tid_out, char** name_out);
        extern void GetStackTraceOnBorrow(slot_t slot, void** pcs[], size_t* size);
        extern void GetThreadInfoOnReturn(slot_t slot, pid_t* tid_out, char** name_out);
        extern void GetStackTraceOnReturn(slot_t slot, void** pcs[], size_t* size);
    }
}


#endif //__MEMGUARD_PAGEPOOL_H__
