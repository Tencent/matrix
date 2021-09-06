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

extern void xhook_block_refresh();

extern void xhook_unblock_refresh();

extern int xhook_grouped_register(int group_id, const char *pathname_regex_str, const char *symbol,
                                  void *new_func, void **old_func) XHOOK_EXPORT;

extern int xhook_grouped_ignore(int group_id, const char *pathname_regex_str, const char *symbol) XHOOK_EXPORT;

extern void* xhook_elf_open(const char *path) XHOOK_EXPORT;

extern int xhook_got_hook_symbol(void* h_lib, const char* symbol, void* new_func, void** old_func) XHOOK_EXPORT;

extern void xhook_elf_close(void *h_lib) XHOOK_EXPORT;

extern int xhook_export_symtable_hook(const char* owner_lib_name, const char* symbol_name, void* handler,
                                      void** original_address) XHOOK_EXPORT;

#ifdef __cplusplus
}
#endif

#endif //XHOOK_EXT_H
