//
// Created by tomystang on 2020/11/26.
//

#ifndef __MEMGUARD_MEMORY_H__
#define __MEMGUARD_MEMORY_H__


#include <cstddef>

namespace memguard {
    namespace memory {
        extern void* MapMemArea(size_t size, const char* region_name, void* hint = nullptr, bool force_hint = false);
        extern void UnmapMemArea(void* addr, size_t size);
        extern void MarkAreaReadWrite(void* start, size_t size);
        extern void MarkAreaInaccessible(void* start, size_t size);
        extern size_t GetPageSize();
    }
}


#endif //__MEMGUARD_MEMORY_H__
