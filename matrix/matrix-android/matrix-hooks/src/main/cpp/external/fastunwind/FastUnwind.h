//
// Created by tomystang on 2020/12/15.
//

#ifndef __MEMGUARD_FASTUNWIND_H__
#define __MEMGUARD_FASTUNWIND_H__

#include <cstddef>

namespace fastunwind {
    extern __attribute__((no_sanitize("address", "hwaddress"))) int Unwind(void** pcs, size_t max_count);
    extern __attribute__((no_sanitize("address", "hwaddress"))) int Unwind(void* ucontext, void** pcs, size_t max_count);
}


#endif //__MEMGUARD_FASTUNWIND_H__
