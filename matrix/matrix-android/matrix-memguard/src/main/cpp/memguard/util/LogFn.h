//
// Created by YinSheng Tang on 2022/1/23.
//

#ifndef __MEMGUARD_LOGFN_H__
#define __MEMGUARD_LOGFN_H__


#include <stdarg.h>

namespace memguard {
    namespace log {
        extern void PrintLog(int level, const char* tag, const char* fmt, ...);
        extern void PrintLogV(int level, const char* tag, const char* fmt, va_list args);
    }
}


#endif //__MEMGUARD_LOGFN_H__
