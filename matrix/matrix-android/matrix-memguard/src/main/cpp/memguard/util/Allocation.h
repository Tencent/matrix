//
// Created by tomystang on 2020/10/16.
//

#ifndef __MEMGUARD_MEMALLOCATION_H__
#define __MEMGUARD_MEMALLOCATION_H__


#include <cstdlib>

namespace memguard {
    namespace allocation {
        extern bool Prepare();
        extern void* Allocate(size_t size);
        extern void* AlignedAllocate(size_t size, size_t alignment);
        extern bool IsAllocatedByThisAllocator(void* ptr);
        extern size_t GetAllocatedSize(void* ptr);
        extern bool Free(void* ptr);
    }
}


#endif //__MEMGUARD_MEMALLOCATION_H__
