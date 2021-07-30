//
// Created by tomystang on 2021/2/3.
//

#ifndef XHOOK_EXT_H
#define XHOOK_EXT_H 1


#include <stddef.h>
#include "xhook.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void* xhook_elf_open(const char *path);

extern int xhook_got_hook_symbol(void* h_lib, const char* symbol, void* new_func, void** old_func);

extern void xhook_elf_close(void *h_lib);

extern int xhook_export_symtable_hook(const char* owner_lib_name, const char* symbol_name, void* handler,
      void** original_address) XHOOK_EXPORT;

#ifdef __cplusplus
}
#endif

#endif //XHOOK_EXT_H
