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
#include "xhook_ext.h"
#include "xh_core.h"
#include "xh_elf.h"
#include "xh_errno.h"

#include "xh_log.h"
#include "xh_util.h"

#ifdef XH_LOG_TAG
  #undef XH_LOG_TAG
  #define XH_LOG_TAG "xhook_ext"
#endif

static int xh_export_symtable_hook(const char* pathname, const void* base_addr, const char* symbol_name, void* handler,
                                   void** original_address) {
    if(NULL == symbol_name || NULL == handler) return XH_ERRNO_INVAL;

    xh_elf_t self = {};
    {
        int error = xh_elf_init(&self, (uintptr_t) base_addr, pathname);
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

static int xhook_find_library_base_addr(const char* owner_lib_name, char path_name_out[PATH_MAX + 1], const void** base_addr_out) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if(fp == NULL) {
        XH_LOG_ERROR("fopen /proc/self/maps failed");
        return XH_ERRNO_ELFINIT;
    }

    char line[512] = {};
    while(fgets(line, sizeof(line), fp)) {
        uintptr_t base_addr = 0;
        char perm[5] = {};
        unsigned long offset = 0;
        int pathname_pos = 0;
        if (sscanf(line, "%"PRIxPTR"-%*lx %4s %lx %*x:%*x %*d%n", &base_addr, perm, &offset, &pathname_pos) != 3) {
            continue;
        }

        //check permission
        if (perm[0] != 'r') continue;
        if (perm[3] != 'p') continue; //do not touch the shared memory

        //check offset
        //
        //We are trying to find ELF header in memory.
        //It can only be found at the beginning of a mapped memory regions
        //whose offset is 0.
        if (0 != offset) continue;

        // Skip normal mmapped region.
        // Some libraries may mmap specific elf file into memory for
        // accessing as a normal file.
        {
            Dl_info info;
            if (dladdr((void*) base_addr, &info) == 0) {
                continue;
            }
        }

        //get pathname
        while (isspace(line[pathname_pos]) && pathname_pos < (int) (sizeof(line) - 1)) {
            pathname_pos += 1;
        }
        if (pathname_pos >= (int) (sizeof(line) - 1)) continue;
        char* pathname = line + pathname_pos;
        size_t pathname_len = strlen(pathname);
        if (0 == pathname_len) continue;
        if (pathname[pathname_len - 1] == '\n') {
            pathname[pathname_len - 1] = '\0';
            pathname_len -= 1;
        }
        if (0 == pathname_len) continue;
        if ('[' == pathname[0]) continue;

        //check pathname
        //if we need to hook this elf?
        char real_suffix[PATH_MAX + 1] = {};
        size_t real_suffix_len = snprintf(real_suffix, sizeof(real_suffix), "/%s", owner_lib_name);
        if (pathname_len < real_suffix_len) {
            continue;
        }
        if (strncmp(pathname + pathname_len - real_suffix_len, real_suffix, real_suffix_len) != 0) {
            continue;
        }

        //check elf header format
        //We are trying to do ELF header checking as late as possible.
        if (0 != xh_elf_check_elfheader(base_addr)) continue;

        XH_LOG_DEBUG("found library, owner_lib_name: %s, path: %s, base: %" PRIxPTR,
                     owner_lib_name, pathname, base_addr);

        if (path_name_out != NULL) {
            strncpy(path_name_out, pathname, PATH_MAX);
        }
        if (base_addr_out != NULL) {
            *base_addr_out = (const void*) base_addr;
        }
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return XH_ERRNO_NOTFND;
}

int xhook_export_symtable_hook(const char* owner_lib_name, const char* symbol_name, void* handler,
                               void** original_address) {
    char path_name[PATH_MAX + 1] = {};
    const void* base_addr = NULL;
    if (xhook_find_library_base_addr(owner_lib_name, path_name, &base_addr) == 0) {
        return xh_export_symtable_hook(path_name, base_addr, symbol_name, handler, original_address);
    } else {
        return XH_ERRNO_NOTFND;
    }
}

void* xhook_find_symbol(const char* owner_lib_name, const char* symbol_name) {
    char path_name[PATH_MAX + 1] = {};
    const void* base_addr = NULL;
    if (xhook_find_library_base_addr(owner_lib_name, path_name, &base_addr) == 0) {
        xh_elf_t self = {};
        {
            int error = xh_elf_init(&self, (uintptr_t) base_addr, path_name);
            if (error != 0) return NULL;
        }

        //find symbol index by symbol name
        uint32_t symidx = 0;
        {
            int error = xh_elf_find_symidx_by_name(&self, symbol_name, &symidx);
            if (error != 0) return NULL;
        }

        ElfW(Sym)* target_sym = self.symtab + symidx;
        return (void*) (self.bias_addr + target_sym->st_value);
    } else {
        return NULL;
    }
}