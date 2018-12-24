/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created by tangyinsheng on 2017/12/7.
//


#include <android/log.h>
#include <linux/elf.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

#include "elf_hook.h"

#define LOG_TAG "Matrix.ELFHOOK"

///////////////////////////////////// Helper Macros Definitions /////////////////////////////////////

#define TRUE 1
#define FALSE 0

#define LOG_DEBUG 1

#define LOG_VERBOSE 0
#define LOG_DEBUG 1
#define LOG_INFO 2
#define LOG_WARN 3
#define LOG_ERROR 4

#define LOCAL_LOG_LEVEL LOG_DEBUG

#ifndef NDEBUG
#if (LOCAL_LOG_LEVEL <= LOG_VERBOSE)
#define LOGV(tag, fmt, args...) do { __android_log_print(ANDROID_LOG_VERBOSE, tag, fmt, ##args); } while(0)
#else
#define LOGV(tag, fmt, args...)
#endif
#if (LOCAL_LOG_LEVEL <= LOG_DEBUG)
#define LOGD(tag, fmt, args...) do { __android_log_print(ANDROID_LOG_DEBUG, tag, fmt, ##args); } while(0)
#else
#define LOGD(tag, fmt, args...)
#endif
#if (LOCAL_LOG_LEVEL <= LOG_INFO)
#define LOGI(tag, fmt, args...) do { __android_log_print(ANDROID_LOG_INFO, tag, fmt, ##args); } while(0)
#else
#define LOGI(tag, fmt, args...)
#endif
#if (LOCAL_LOG_LEVEL <= LOG_WARN)
#define LOGW(tag, fmt, args...) do { __android_log_print(ANDROID_LOG_WARN, tag, fmt, ##args); } while(0)
#else
#define LOGW(tag, fmt, args...)
#endif
#if (LOCAL_LOG_LEVEL <= LOG_ERROR)
#define LOGE(tag, fmt, args...) do { __android_log_print(ANDROID_LOG_ERROR, tag, fmt, ##args); } while(0)
#else
#define LOGE(tag, fmt, args...)
#endif
#else
#define LOGV(tag, fmt, args...)
#define LOGD(tag, fmt, args...)
#define LOGI(tag, fmt, args...)
#define LOGW(tag, fmt, args...)
#define LOGE(tag, fmt, args...)
#endif

#define GOTO_IF(cond, label) do { if ((cond)) goto label; } while (0)
#define SAFELY_FREE(free_func, ptr) do { if (ptr) { free_func((void *) ptr); (ptr) = NULL; } } while (0)
#define SAFELY_ASSIGN_PTR(ptr, val) do { if ((ptr)) *(ptr) = (val); } while (0)

#define TRANSLATE_PFLAG_BIT(flags, pflag_bit, priv_bit) ((flags & pflag_bit) ? priv_bit : 0)
#define PFLAGS_TO_PRIV_BITS(flags) (uint32_t) ((TRANSLATE_PFLAG_BIT(flags, PF_R, PROT_READ) \
                                              | TRANSLATE_PFLAG_BIT(flags, PF_W, PROT_WRITE) \
                                              | TRANSLATE_PFLAG_BIT(flags, PF_X, PROT_EXEC)))


#define max(x, y) ({\
	const typeof(x) _x = (x);\
	const typeof(y) _y = (y);\
	(void)(&_x == &_y);\
	_x > _y ? _x : _y;\
})

///////////////////////////////////// Helper Function Declarations /////////////////////////////////////

static int compileAbiToElfMachine();

static void *get_base_addr(int pid, const char *so_path);

static int verify_elf_header(Elf_Ehdr *ehdr);

static int fill_rest_info(loaded_soinfo *soinfo);

static uint32_t calculate_elf_hash(const char *name);

static int locate_symbol(const loaded_soinfo *soinfo, const char *name, Elf_Sym **sym_out, size_t *sym_idx_out);

static int locate_symbol_linear(const loaded_soinfo *soinfo, const char *name, Elf_Sym **sym_out, size_t *sym_idx_out);

static int locate_symbol_hash(const loaded_soinfo *soinfo, const char *name, Elf_Sym **sym_out, size_t *sym_idx_out);

static int replace_elf_rel_segment(const loaded_soinfo *soinfo, Elf_Addr rel_addr, size_t rel_count,
                                   size_t target_sym_idx, void *new_func, void **original_func);

static void clear_cache(void *addr);

///////////////////////////////////// Implementations /////////////////////////////////////

loaded_soinfo *elfhook_open(const char *so_path) {
    void *base_addr = get_base_addr(0, so_path);
    if (!base_addr) {
        LOGE(LOG_TAG, "failure to get base address of so: %s", so_path);
        return NULL;
    }

    loaded_soinfo *soinfo = (loaded_soinfo *) malloc(sizeof(loaded_soinfo));
    if (!soinfo) {
        LOGE(LOG_TAG, "failure to allocate loaded_soinfo struct for %s.", so_path);
        return NULL;
    }

    do {
        memset(soinfo, 0, sizeof(loaded_soinfo));
        size_t so_path_len = strlen(so_path);
        char *so_name = (char *) malloc(so_path_len + 1);
        if (!so_name) {
            LOGE(LOG_TAG, "failure to allocate space for storing name of so.");
            goto failure;
        }
        strncpy(so_name, so_path, so_path_len);
        so_name[so_path_len] = '\0';
        soinfo->name = (const char *) so_name;

        soinfo->base = (Elf_Addr) base_addr;

        Elf_Ehdr *ehdr = (Elf_Ehdr *) base_addr;
        if (!verify_elf_header(ehdr)) {
            LOGE(LOG_TAG, "failure to pass elf header verification for %s", so_path);
            goto failure;
        }
        soinfo->ehdr = ehdr;

        Elf_Phdr *phdr = (Elf_Phdr *) (base_addr + ehdr->e_phoff);
        soinfo->phdr = phdr;

        Elf_Shdr *shdr = (Elf_Shdr *) (base_addr + ehdr->e_shoff);
        soinfo->shdr = shdr;

        if (!fill_rest_info(soinfo)) {
            LOGE(LOG_TAG, "failure to fill rest so info for %s", so_path);
            goto failure;
        }
    } while (0);

    goto bail;

    failure:
    SAFELY_FREE(free, soinfo->name);
    SAFELY_FREE(free, soinfo);
    bail:
    return soinfo;
}

int elfhook_replace(const loaded_soinfo *soinfo, const char *func_name, void *new_func, void **original_func) {
    int result = FALSE;
    Elf_Sym *target_sym = NULL;
    size_t target_sym_idx = 0;
    if (!locate_symbol(soinfo, func_name, &target_sym, &target_sym_idx)) {
        LOGE(LOG_TAG, "failure to find symbol for function: %s @ %s", func_name, soinfo->name);
        return FALSE;
    }

    LOGI(LOG_TAG, "found symbol for function: %s, index: %zu @ %s", func_name, target_sym_idx, soinfo->name);

    result = replace_elf_rel_segment(soinfo, soinfo->rel, soinfo->rel_count, target_sym_idx, new_func,
                                     original_func);
    if (!result) {
        LOGD(LOG_TAG, "cannot replace function %s @ %s in rel segment, try other segment next.", func_name, soinfo->name);
        result = replace_elf_rel_segment(soinfo, soinfo->plt_rel, soinfo->plt_rel_count, target_sym_idx, new_func,
                                         original_func);
    }

    if (!result) {
        LOGW(LOG_TAG, "cannot hook function %s @ %s.", func_name, soinfo->name);
    } else {
        LOGI(LOG_TAG, "target function [%s] @ %s is hooked successfully, new_addr: %p", func_name, soinfo->name, new_func);
    }
    return result;
}

void elfhook_close(loaded_soinfo *soinfo) {
    if (soinfo) {
        SAFELY_FREE(free, soinfo->name);
        memset(soinfo, 0xAC, sizeof(loaded_soinfo));
        free(soinfo);
    }
}

static void *get_base_addr(int pid, const char *so_path) {
    unsigned long result = 0;
    char maps_path[256];
    if (pid == 0) {
        strncpy(maps_path, "/proc/self/maps", sizeof(maps_path));
    } else {
        snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    }
    FILE *maps_fp = fopen(maps_path, "rb");
    GOTO_IF(!maps_fp, bail);

    do {
        char maps_line[512];
        while (fgets(maps_line, sizeof(maps_line) - 1, maps_fp) != NULL) {
            char *first_bar_pos = strchr(maps_line, '-');
            if (!first_bar_pos) {
                LOGV(LOG_TAG, "skip bad maps line: %s", maps_line);
                continue;
            }
            unsigned int addr_size = (unsigned int) (first_bar_pos - (char *) maps_line);
            char *privbits = first_bar_pos + addr_size
                             + 1 /* barchar itself */
                             + 1 /* space before privbit */;
            if (privbits[0] != 'r' || privbits[2] != 'x') {
                LOGV(LOG_TAG, "skip maps line with unconcerned privilege: %s", maps_line);
                continue;
            }
            if (strstr(maps_line, so_path)) {
                *first_bar_pos = '\0';
                result = strtoul(maps_line, NULL, 16);
                LOGD(LOG_TAG, "found maps line for so [%s], base addr: %lx", so_path, result);
                break;
            }
        }
    } while (0);

    bail:
    SAFELY_FREE(fclose, maps_fp);
    return (void *) result;
}

static int compileAbiToElfMachine() {
#if defined(__arm__)
    return EM_ARM;
#elif defined(__aarch64__)
    return EM_AARCH64;
#elif defined(__i386__)
    return EM_386;
#elif defined(__mips__)
    return EM_MIPS;
#elif defined(__x86_64__)
    return EM_X86_64;
#endif
}

static int verify_elf_header(Elf_Ehdr *ehdr) {
    uint32_t magic = *((uint32_t *) ehdr->e_ident);
    if (memcmp(&magic, ELFMAG, SELFMAG) != 0) {
        LOGE(LOG_TAG, "bad elf magic: %x", magic);
        return FALSE;
    }
    int ei_class = ehdr->e_ident[EI_CLASS];
#if defined(__LP64__)
    if (ei_class != ELFCLASS64) {
        LOGE(LOG_TAG, "expected ELFCLASS64(%d) format, but %d format found.", ELFCLASS64, ei_class);
        return FALSE;
    }
#else
    if (ei_class != ELFCLASS32) {
        LOGE(LOG_TAG, "expected ELFCLASS32(%d) format, but %d format found.", ELFCLASS32, ei_class);
        return FALSE;
    }
#endif
    int ei_data = ehdr->e_ident[EI_DATA];
    if (ei_data != ELFDATA2LSB) {
        LOGE(LOG_TAG, "expected endian ELFDATA2LSB (%d), but %d found.", ELFDATA2LSB, ei_data);
        return FALSE;
    }
    int e_version = ehdr->e_version;
    if (e_version != EV_CURRENT) {
        LOGE(LOG_TAG, "expected version %d, but %d found.", EV_CURRENT, e_version);
        return FALSE;
    }
    int e_machine = ehdr->e_machine;
    int expected_machine = compileAbiToElfMachine();
    if (e_machine != expected_machine) {
        LOGE(LOG_TAG, "expected elf machine %d, but %d found.", expected_machine, e_machine);
        return FALSE;
    } else if (expected_machine == EM_MIPS) {
        LOGE(LOG_TAG, "mips so is not supported.");
    }
    return TRUE;
}

static int fill_rest_info(loaded_soinfo *soinfo) {
    Elf_Ehdr *ehdr = soinfo->ehdr;
    Elf_Phdr *phdr = soinfo->phdr;

    do {
        Elf_Addr bias = 0;
        uint32_t privilege = 0;
        void *phdr_bound = phdr + ehdr->e_phnum;
        for (Elf_Phdr *curr_phdr = phdr; (void *) curr_phdr < phdr_bound; ++curr_phdr) {
            if (curr_phdr->p_type == PT_LOAD) {
                bias = ((Elf_Addr) ehdr) + curr_phdr->p_offset - curr_phdr->p_vaddr;
                privilege = PFLAGS_TO_PRIV_BITS(curr_phdr->p_flags);
                break;
            }
        }
        if (bias == 0) {
            LOGE(LOG_TAG, "failure to get bias value.");
            goto failure;
        }
        soinfo->bias = bias;
        soinfo->privilege = privilege;

        Elf_Dyn *dyn = NULL;
        Elf_Word ndyn = 0;
        for (Elf_Phdr *curr_phdr = phdr; (void *) curr_phdr < phdr_bound; ++curr_phdr) {
            if (curr_phdr->p_type == PT_DYNAMIC) {
                dyn = (Elf_Dyn *) (bias + curr_phdr->p_vaddr);
                ndyn = (Elf_Word) (curr_phdr->p_memsz / sizeof(Elf_Dyn));
                break;
            }
        }
        if (!dyn) {
            LOGE(LOG_TAG, "failure to get dynamic segment.");
            goto failure;
        }
        soinfo->dyn = dyn;
        soinfo->dyn_count = ndyn;

        void *dyn_bound = dyn + ndyn;
        int use_rela = FALSE;
        for (Elf_Dyn *curr_dyn = dyn; (void *) curr_dyn < dyn_bound; ++curr_dyn) {
            if (curr_dyn->d_tag == DT_PLTREL) {
                use_rela = (curr_dyn->d_un.d_val == DT_RELA);
                break;
            }
        }
        soinfo->use_rela = use_rela;

        for (Elf_Dyn *curr_dyn = dyn; (void *) curr_dyn < dyn_bound; ++curr_dyn) {
            switch (curr_dyn->d_tag) {
                case DT_SYMTAB:
                    soinfo->sym = (Elf_Sym *) (bias + curr_dyn->d_un.d_ptr);
                    break;
                case DT_STRTAB:
                    soinfo->symstr = (const char *) (bias + curr_dyn->d_un.d_ptr);
                    break;
                case DT_JMPREL:
                    soinfo->plt_rel = bias + curr_dyn->d_un.d_ptr;
                    break;
                case DT_PLTRELSZ:
                    if (soinfo->use_rela) {
                        soinfo->plt_rel_count = curr_dyn->d_un.d_val / sizeof(Elf_Rela);
                    } else {
                        soinfo->plt_rel_count = curr_dyn->d_un.d_val / sizeof(Elf_Rel);
                    }
                    break;
                case DT_REL:
                case DT_RELA:
                    soinfo->rel = bias + curr_dyn->d_un.d_ptr;
                    break;
                case DT_RELSZ:
                case DT_RELASZ:
                    if (soinfo->use_rela) {
                        soinfo->rel_count = curr_dyn->d_un.d_val / sizeof(Elf_Rela);
                    } else {
                        soinfo->rel_count = curr_dyn->d_un.d_val / sizeof(Elf_Rel);
                    }
                    break;
                case DT_HASH: {
                    Elf_Addr addr = bias + curr_dyn->d_un.d_ptr;
                    soinfo->nbucket = ((size_t *) addr)[0];
                    soinfo->nchain = ((size_t *) addr)[1];
                    soinfo->bucket = (unsigned *) (addr + 8);
                    soinfo->chain = (unsigned *) (addr + 8 + (soinfo->nbucket << 2));
                    soinfo->sym_count = soinfo->nchain;
                    break;
                }
                case DT_GNU_HASH: {
                    soinfo->sym_count = ((uint32_t *) (bias + curr_dyn->d_un.d_ptr))[1];
                    break;
                }
            }
        }

        if (!soinfo->sym || !soinfo->symstr) {
            LOGE(LOG_TAG, "symbol string table or symbol table is not found, sym_tbl: %p, symstr_tbl: %p",
                 soinfo->sym, soinfo->symstr);
            goto failure;
        }
    } while (0);

    goto bail;

    failure:
    return FALSE;
    bail:
    return TRUE;
}

static int locate_symbol_linear(const loaded_soinfo *soinfo, const char *name, Elf_Sym **sym_out, size_t *sym_idx_out) {
    LOGD(LOG_TAG, "locating symbol %s by linear searching.", name);

    for (size_t idx = 0; idx < soinfo->sym_count; ++idx) {
        Elf_Sym *curr_sym = soinfo->sym + idx;
        const char *sym_name = soinfo->symstr + curr_sym->st_name;

        int sym_type = ELF_ST_TYPE(curr_sym->st_info);
        if (sym_type != STT_FUNC) {
            LOGV(LOG_TAG, "skip symbol %s with index %zu @ %s since it's not a function.", sym_name, n, soinfo->name);
            continue;
        }

        LOGV(LOG_TAG, "current sym from symtab: %s @ %zu, target: %s", sym_name, idx, name);
        if (strncmp(sym_name, name, max(strlen(sym_name), strlen(name))) == 0) {
            SAFELY_ASSIGN_PTR(sym_out, curr_sym);
            SAFELY_ASSIGN_PTR(sym_idx_out, idx);
            return TRUE;
        }
    }

    LOGD(LOG_TAG, "locate symbol by linear searching done, nothing found.");

    return FALSE;
}

static uint32_t calculate_elf_hash(const char *name) {
    const uint8_t *name_bytes = (const uint8_t *) (name);
    uint32_t h = 0;
    uint32_t g = 0;
    while (*name_bytes) {
        h = (h << 4) + *name_bytes++;
        g = h & 0xf0000000;
        h ^= g;
        h ^= g >> 24;
    }
    return h;
}

static int locate_symbol_hash(const loaded_soinfo *soinfo, const char *name, Elf_Sym **sym_out, size_t *sym_idx_out) {
    int name_hash = calculate_elf_hash(name);
    LOGD(LOG_TAG, "locating symbol %s by elf hash, name hashcode: %d", name, name_hash);

    size_t nbucket = soinfo->nbucket;
    unsigned *bucket = soinfo->bucket;
    unsigned *chain = soinfo->chain;
    for (unsigned n = bucket[name_hash % nbucket]; n != 0; n = chain[n]) {
        Elf_Sym *curr_sym = soinfo->sym + n;
        const char *sym_name = soinfo->symstr + curr_sym->st_name;

        int sym_type = ELF_ST_TYPE(curr_sym->st_info);
        if (sym_type != STT_FUNC) {
            LOGV(LOG_TAG, "skip symbol %s with index %d @ %s since it's not a function.", sym_name, n, soinfo->name);
            continue;
        }

        LOGD(LOG_TAG, "current sym from bucket: %s @ %d, target: %s", sym_name, n, name);
        if (strncmp(sym_name, name, max(strlen(sym_name), strlen(name))) == 0) {
            SAFELY_ASSIGN_PTR(sym_out, curr_sym);
            SAFELY_ASSIGN_PTR(sym_idx_out, n);
            return TRUE;
        }
    }

    LOGD(LOG_TAG, "locate symbol by hash done, nothing found.");

    return FALSE;
}

static int locate_symbol(const loaded_soinfo *soinfo, const char *name, Elf_Sym **sym_out, size_t *sym_idx_out) {
    int result = FALSE;
    if (soinfo->bucket && soinfo->chain) {
        result = locate_symbol_hash(soinfo, name, sym_out, sym_idx_out);
    }
    if (!result) {
        result = locate_symbol_linear(soinfo, name, sym_out, sym_idx_out);
    }
    return result;
}

static void clear_cache(void *addr) {
    unsigned long soff = ((unsigned long) addr) & PAGE_MASK;
    unsigned long eoff = soff + getpagesize();
    syscall(0xf0002, soff, eoff);
}

static int replace_elf_rel_segment(const loaded_soinfo *soinfo, Elf_Addr rel_addr, size_t rel_count,
                                   size_t target_sym_idx, void *new_func, void **original_func) {
    int result = FALSE;
    if (!rel_addr || rel_count == 0) {
        return result;
    }
    for (size_t i = 0; i < rel_count; ++i) {
        unsigned long r_info = 0;
        Elf_Addr r_offset = 0;
        if (soinfo->use_rela) {
            Elf_Rela *rel_item = (((Elf_Rela *) rel_addr) + i);
            r_info = rel_item->r_info;
            r_offset = rel_item->r_offset;
        } else {
            Elf_Rel *rel_item = (((Elf_Rel *) rel_addr) + i);
            r_info = rel_item->r_info;
            r_offset = rel_item->r_offset;
        }
        if (ELF_R_SYM(r_info) == target_sym_idx) {
            void *addr_offset = (void *) (soinfo->bias + r_offset);
            void *original_func_addr = *((void **) addr_offset);
            if (original_func_addr == new_func) {
                LOGW(LOG_TAG, "target function has been replaced, new addr: %p", new_func);
                result = TRUE;
                break;
            }
            uint32_t orig_priv = soinfo->privilege;
            uint32_t new_priv = (orig_priv | PROT_WRITE) & (~PROT_EXEC);
            if (mprotect((void *) ((unsigned long) addr_offset & PAGE_MASK), PAGE_SIZE, new_priv) != 0) {
                int errcode = errno;
                LOGE(LOG_TAG, "failure to change privilege of addr: %p, errcode: %d", addr_offset, errcode);
                break;
            }

            SAFELY_ASSIGN_PTR(original_func, original_func_addr);
            *(void **) addr_offset = new_func;
            clear_cache(addr_offset);

            mprotect((void *) ((unsigned long) addr_offset & PAGE_MASK), PAGE_SIZE, orig_priv);

            result = TRUE;
            break;
        }
    }
    return result;
}
