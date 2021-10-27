//
// Created by tomystang on 2020/10/15.
//

#ifndef __MEMGUARD_UNWIND_H__
#define __MEMGUARD_UNWIND_H__


#include <cstdlib>
#include <string>

namespace memguard {
    namespace unwind {
        extern bool Initialize();
        extern int UnwindStack(void** pcs, size_t max_count);
        extern int UnwindStack(void* ucontext, void** pcs, size_t max_count);
        extern char* GetStackElementDescription(const void* pc, char* desc, size_t max_length);
    }
}


#endif //__MEMGUARD_UNWIND_H__
