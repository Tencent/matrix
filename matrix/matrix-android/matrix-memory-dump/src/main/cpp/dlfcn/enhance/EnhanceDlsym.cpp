/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
// Created by Yves on 2020/7/15.
//

#include "EnhanceDlsym.h"

#include <cstdio>
#include <elf.h>
#include <cinttypes>
#include <android/log.h>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include <set>

#include "../../internal/log.h"

#define TAG "Matrix.EnhanceDl"

namespace enhance {

    static std::set<DlInfo *> m_opened_info;
    static std::mutex         m_dl_mutex;
    static std::map<void *, ElfW(Sym) *> m_founded_symtab;

    static inline bool end_with(std::string const &value, std::string const &ending) {
        if (ending.size() > value.size()) {
            return false;
        }
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    static inline bool start_with(std::string const &value, std::string const &starting) {
        if (starting.size() > value.size()) {
            return false;
        }
        return std::equal(starting.begin(), starting.end(), value.begin());
    }

    static std::string prepare_filename(const char *__filename) {
        assert(__filename != nullptr);
        std::stringstream ss;

        if (!start_with(__filename, "/")) {
            ss << "/";
            if (!start_with(__filename, "lib")) {
                ss << "lib";
            }
        }

        ss << __filename;

        if (!end_with(__filename, ".so")) {
            ss << ".so";
        }

        return ss.str();
    }

    static ElfW(Phdr) *get_first_segment_by_type_offset(DlInfo &__info, ElfW(Word) __type,
                                                        ElfW(Off) __offset) {
        ElfW(Phdr) *phdr;

        for (phdr = __info.phdr; phdr < __info.phdr + __info.ehdr->e_phnum; phdr++) {
            if (phdr->p_type == __type && phdr->p_offset == __offset) {
                return phdr;
            }
        }
        return nullptr;
    }

    static int check_loaded_so(void *addr) {
        Dl_info stack_info;
        return dladdr(addr, &stack_info);
    }

    static bool parse_maps(const std::string &__file_name, DlInfo &__info) {
        bool          found         = false;
        uintptr_t     map_base_addr = 0;
        char          map_perm[5];
        unsigned long map_offset    = 0;
        char          map_file_name[512];

        std::ifstream    input("/proc/self/maps");
        std::string aline;

        while (getline(input, aline)) {
            if (sscanf(aline.c_str(),
                       "%" PRIxPTR "-%*lx %4s %lx %*x:%*x %*d%s",
                       &map_base_addr,
                       map_perm,
                       &map_offset,
                       map_file_name) != 4) {
                continue;
            }

//            _debug_log(TAG, "%s", aline.c_str());

            if (end_with(map_file_name, __file_name)
                && 0 != check_loaded_so((void *) map_base_addr)) {
                found = true;
                _debug_log(TAG, "found entry [%s]", aline.c_str());
                break;
            }
        }

        if (!found || map_perm[0] != 'r' || map_perm[3] != 'p') {
            _error_log(TAG, "maps entry not found, %d, %c, %c", found, map_perm[0], map_perm[3]);
            return false;
        }

        __info.pathname  = map_file_name;
        __info.base_addr = map_base_addr;
        __info.ehdr      = reinterpret_cast<ElfW(Ehdr) *>(map_base_addr);
        __info.phdr      = reinterpret_cast<ElfW(Phdr) *>(map_base_addr + __info.ehdr->e_phoff);

        //find the first load-segment with offset 0
        ElfW(Phdr)    *phdr0 = get_first_segment_by_type_offset(__info, PT_LOAD, 0);
        if (!phdr0) {
            _error_log(TAG, "Can NOT found the first load segment. %s", map_file_name);
            return false;
        }

        //save load bias addr
        if (__info.base_addr < phdr0->p_vaddr) {
            _error_log(TAG, "base_addr < phdr0->p_vaddr");
            return false;
        }
        __info.bias_addr = __info.base_addr - phdr0->p_vaddr;
        _debug_log(TAG, "bias_addr = %p, bias = %p", (void *)__info.bias_addr, (void *)phdr0->p_vaddr);

        return true;
    }

    static bool parse_section_header(DlInfo &__info) {

        int fd = open(__info.pathname.c_str(), O_RDONLY | O_CLOEXEC);
        if (-1 == fd) {
            _error_log(TAG, "open file failed: %s", strerror(errno));
            return false;
        }

        off_t elf_size  = lseek(fd, 0, SEEK_END);
        if (-1 == elf_size) {
            close(fd);
            return false;
        }
        ElfW(Ehdr) *elf = static_cast<ElfW(Ehdr) *>(mmap(nullptr, elf_size, PROT_READ, MAP_SHARED, fd,
                                                         0));
        // TODO check elf header ?

        ElfW(Shdr) *shdr = reinterpret_cast<ElfW(Shdr) *>(((uintptr_t) elf) + elf->e_shoff);

        ElfW(Shdr) *shdr_end = reinterpret_cast<ElfW(Shdr) *>(((uintptr_t) shdr) +
                                                              elf->e_shentsize * elf->e_shnum);

        ElfW(Shdr) *shdr_shstrtab = reinterpret_cast<ElfW(Shdr) *>((uintptr_t) shdr +
                                                                   elf->e_shstrndx *
                                                                   elf->e_shentsize);

        auto shstr = reinterpret_cast<const char *>(((uintptr_t) elf) +
                                                    shdr_shstrtab->sh_offset);

        // SHT_SYMTAB 和 SHT_STRTAB 通常在 section header table 的末尾, 所以倒序遍历
        short flag = 0b11;
        size_t count = 0;
        __info.symtab_num = 0;
        __info.strtab_size = 0;
        do {
            shdr_end--;

            switch (shdr_end->sh_type) {

                case SHT_SYMTAB:
                    _debug_log(TAG, "SHT_SYMTAB[%zu]", count++);
                    __info.symtab = static_cast<ElfW(Sym) *>(malloc(shdr_end->sh_size));
                    memcpy(__info.symtab,
                           reinterpret_cast<const void *>(((uintptr_t) elf) + shdr_end->sh_offset),
                           shdr_end->sh_size);
                    __info.symtab_num = shdr_end->sh_size / sizeof(ElfW(Sym));

                    flag &= 0b01;
                    break;

                case SHT_STRTAB:
                    _debug_log(TAG, "SHT_STRTAB[%zu]", count++);

                    if (0 == strcmp(shstr + shdr_end->sh_name, ".strtab")) {
                        __info.strtab      = static_cast<char *>(malloc(shdr_end->sh_size));
                        __info.strtab_size = shdr_end->sh_size;
                        memcpy(__info.strtab,
                               reinterpret_cast<const void *>(((uintptr_t) elf) +
                                                              shdr_end->sh_offset),
                               shdr_end->sh_size);

                        flag &= 0b10;
                    }

                    break;
            }

            _debug_log(TAG, "flag = %d", flag);

        } while (flag && shdr_end != shdr);
//        for (; shdr < shdr_end; shdr++) {
//
//            switch (shdr->sh_type) {
//
//                case SHT_SYMTAB:
//                    meta__.symtab = static_cast<ElfW(Sym) *>(malloc(shdr->sh_size));
//                    memcpy(meta__.symtab,
//                           reinterpret_cast<const void *>(((uintptr_t) elf) + shdr->sh_offset), shdr->sh_size);
//                    meta__.symtab_num = shdr_end->sh_size / sizeof(ElfW(Sym));
//                    break;
//
//                case SHT_STRTAB:
//
//                    if (0 == strcmp(shstr + shdr->sh_name,".strtab")) {
//                        meta__.strtab = static_cast<char *>(malloc(shdr->sh_size));
//                        memcpy(meta__.strtab,
//                               reinterpret_cast<const void *>(((uintptr_t) elf) + shdr->sh_offset), shdr->sh_size);
//                    }
//
//                    break;
//            }
//        }
        _debug_log(TAG, "got symtab size = %d", __info.symtab_num);

        munmap(elf, elf_size);
        close(fd);

        return true;
    }

    static bool load(const std::string &__file_name, DlInfo &__info) {
        return parse_maps(__file_name, __info) && parse_section_header(__info);
    }

    void *dlopen(const char *__file_name, int __flag) {
        std::lock_guard<std::mutex> lock(m_dl_mutex);

        if (!__file_name) {
            return nullptr;
        }

        std::string &&suffix_name = prepare_filename(__file_name);
        _debug_log(TAG, "final filename = %s", suffix_name.c_str());

        auto info = new DlInfo;

        if (!load(suffix_name, *info)) {
            delete info;
            return nullptr;
        }

        m_opened_info.emplace(info);

        return info;
    }

    int dlclose(void *__handle) {
        std::lock_guard<std::mutex> lock(m_dl_mutex);

        if (__handle) {
            auto info = static_cast<DlInfo *>(__handle);
            m_opened_info.erase(info);
            free(info->strtab);
            free(info);

            std::map<void *, ElfW(Sym) *> empty;
            empty.swap(m_founded_symtab);
            empty.clear();
        }
        return 0;
    }

    // TODO dlsym object and func
    void *dlsym(void *__handle, const char *__symbol) {
        std::lock_guard<std::mutex> lock(m_dl_mutex);

        if (!__handle) {
            return nullptr;
        }
        auto info = static_cast<DlInfo *>(__handle);
        if (!m_opened_info.count(info)) {
            return nullptr;
        }



        ElfW(Sym) *symtab_end = info->symtab + info->symtab_num;
        ElfW(Sym) *symtab_idx = info->symtab;

        for (; symtab_idx < symtab_end; symtab_idx++) {

            if (info->strtab_size <= symtab_idx->st_name) {
                _debug_log(TAG, "context.strtabsz = %d, symtab_idx->st_name = %d",
                        info->strtab_size, symtab_idx->st_name);
            }
            assert (info->strtab_size > symtab_idx->st_name);

            std::string sym_name(info->strtab + symtab_idx->st_name);
            if (sym_name == __symbol) {
                _debug_log(TAG, "st_value=%x", symtab_idx->st_value);
                uintptr_t found_sym_addr = symtab_idx->st_value + info->bias_addr;
                if (check_loaded_so((void *)found_sym_addr) != 0) {
                    auto res = reinterpret_cast<void *>(found_sym_addr);
                    m_founded_symtab[res] = symtab_idx;
                    return res;
                }
            }
        }

        return nullptr;
    }

    size_t dlsizeof(void *__addr) {

        if (m_founded_symtab.count(__addr)) {
            return m_founded_symtab[__addr]->st_size;
        }

        return -1;
    }
}

