//
// Created by tomystang on 2020/10/15.
//

#ifndef __MEMGUARD_HOOK_H__
#define __MEMGUARD_HOOK_H__


#include <vector>
#include <string>

namespace memguard {
    extern bool BeginHook();
    extern bool DoHook(const char* pathname_regex, const char* sym_name, void* handler_func, void** original_func);
    extern bool EndHook(const std::vector<std::string>& ignore_pathname_regex_list);
    extern bool UpdateHook();
}


#endif //__MEMGUARD_HOOK_H__
