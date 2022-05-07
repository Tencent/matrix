//
// Created by tomystang on 2021/2/3.
//

#include <ctype.h>
#include <stdio.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <asm/mman.h>
#include <regex.h>
#include <malloc.h>
#include <semi_dlfcn.h>
#include "xhook_ext.h"
#include "xh_core.h"
#include "xh_elf.h"
#include "xh_errno.h"

#include "xh_log.h"
#include "xh_util.h"
#include "queue.h"
#include "tree.h"

#ifdef XH_LOG_TAG
  #undef XH_LOG_TAG
  #define XH_LOG_TAG "xhook_ext"
#endif

void xhook_block_refresh()
{
    xh_core_block_refresh();
}

void xhook_unblock_refresh()
{
    xh_core_unblock_refresh();
}

int xhook_grouped_register(int group_id, const char *pathname_regex_str, const char *symbol, void *new_func,
        void **old_func)
{
    return xh_core_grouped_register(group_id, pathname_regex_str, symbol, new_func, old_func);
}

int xhook_grouped_ignore(int group_id, const char *pathname_regex_str, const char *symbol)
{
    return xh_core_grouped_ignore(group_id, pathname_regex_str, symbol);
}

void* xhook_elf_open(const char *path)
{
    return xh_core_elf_open(path);
}

int xhook_got_hook_symbol(void* h_lib, const char* symbol, void* new_func, void** old_func)
{
    return xh_core_got_hook_symbol(h_lib, symbol, new_func, old_func);
}

void xhook_elf_close(void *h_lib)
{
    xh_core_elf_close(h_lib);
}

static int xh_export_symtable_hook(
        const char* pathname,
        const void* bias_addr,
        ElfW(Phdr)* phdrs,
        ElfW(Half) phdr_count,
        const char* symbol_name,
        void* handler,
        void** original_address
) {
    if(NULL == symbol_name || NULL == handler) return XH_ERRNO_INVAL;

    xh_elf_t self = {};
    {
        int error = xh_elf_init(&self, (uintptr_t) bias_addr, phdrs, phdr_count, pathname);
        if (error != 0) return error;
    }
    XH_LOG_INFO("hooking %s in %s using export table hook.\n", symbol_name, pathname);

    //find symbol index by symbol name
    uint32_t symidx = 0;
    {
        int error = xh_elf_find_symidx_by_name(&self, symbol_name, &symidx);
        if (error != 0) return error;
    }

    ElfW(Sym)* target_sym = self.symtab + symidx;
    void* old_symaddr = (void*) target_sym->st_value;
    if (original_address != NULL) {
        *original_address = old_symaddr;
    }
    uint32_t old_prot = PROT_NONE;
    int error = xh_util_get_addr_protect((uintptr_t) &target_sym->st_value, pathname, &old_prot);
    if (error != 0) {
        XH_LOG_ERROR("Fail to get original addr privilege flags. addr: %" PRIxPTR,
                     (uintptr_t) &target_sym->st_value);
        return error;
    }
    error = xh_util_set_addr_protect((uintptr_t) &target_sym->st_value, PROT_READ | PROT_WRITE);
    if (error != 0) {
        XH_LOG_ERROR("Fail to make addr be able to read and write. addr: %" PRIxPTR,
                     (uintptr_t) &target_sym->st_value);
        return error;
    }
    target_sym->st_value = ((ElfW(Addr)) handler - self.bias_addr);
    xh_util_flush_instruction_cache((uintptr_t) &target_sym->st_value);
    xh_util_set_addr_protect((uintptr_t) &target_sym->st_value, old_prot);

    XH_LOG_INFO("Successfully hook symbol: %s at %s, old_sym_addr: %p, handler_addr: %p",
          symbol_name, pathname, old_symaddr, handler);
    return 0;
}

typedef struct found_lib_info {
    struct {
        const char* owner_lib_name;
    } args_in;

    struct {
        char path_name[PATH_MAX + 1];
        const void* bias_addr;
        ElfW(Phdr)* phdrs;
        ElfW(Half) phdr_count;
    } args_out;
} found_lib_info_t;

#define MAPS_ITER_RET_NOTFOUND 0
#define MAPS_ITER_RET_FOUND 1

static int find_owner_library_cb(struct dl_phdr_info* info, size_t info_size, void* data) {
    found_lib_info_t* found_lib_info = (found_lib_info_t*) data;

    const char* owner_lib_name = found_lib_info->args_in.owner_lib_name;
    size_t owner_name_len = strlen(owner_lib_name);
    if (owner_name_len == 0) return MAPS_ITER_RET_NOTFOUND;

    char real_suffix[PATH_MAX + 1];
    if (owner_lib_name[0] != '/') {
        real_suffix[0] = '/';
        strncpy(real_suffix + 1, owner_lib_name, PATH_MAX);
        ++owner_name_len;
    } else {
        strncpy(real_suffix, owner_lib_name, PATH_MAX);
    }
    if (owner_name_len > PATH_MAX) owner_name_len = PATH_MAX;
    real_suffix[owner_name_len] = '\0';

    XH_LOG_DEBUG("find_owner_library_cb: curr_pathname: %s, real_suffix: %s", info->dlpi_name, real_suffix);

    size_t curr_pathname_len = strlen(info->dlpi_name);
    if (strncmp(info->dlpi_name + curr_pathname_len - owner_name_len, real_suffix, owner_name_len) == 0) {
        strcpy(found_lib_info->args_out.path_name, info->dlpi_name);
        found_lib_info->args_out.bias_addr = (const void*) info->dlpi_addr;
        found_lib_info->args_out.phdrs = (ElfW(Phdr)*) info->dlpi_phdr;
        found_lib_info->args_out.phdr_count = info->dlpi_phnum;
        XH_LOG_INFO("Found owner lib '%s' by suffix '%s'.", info->dlpi_name, real_suffix);
        return MAPS_ITER_RET_FOUND;
    } else {
        return MAPS_ITER_RET_NOTFOUND;
    }
}

int xhook_export_symtable_hook(const char* owner_lib_name, const char* symbol_name, void* handler,
                               void** original_address) {
    found_lib_info_t found_lib_info = {};
    found_lib_info.args_in.owner_lib_name = owner_lib_name;
    switch (semi_dl_iterate_phdr(find_owner_library_cb, &found_lib_info)) {
        case MAPS_ITER_RET_FOUND: {
            return xh_export_symtable_hook(
                    found_lib_info.args_out.path_name,
                    found_lib_info.args_out.bias_addr,
                    found_lib_info.args_out.phdrs,
                    found_lib_info.args_out.phdr_count,
                    symbol_name,
                    handler,
                    original_address
            );
        }
        case MAPS_ITER_RET_NOTFOUND: {
            return XH_ERRNO_NOTFND;
        }
        case XH_ERRNO_NOMEM: {
            return XH_ERRNO_NOMEM;
        }
        default: {
            return XH_ERRNO_UNKNOWN;
        }
    }
}