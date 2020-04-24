//
// Created by Yves on 2020-04-09.
//

#include <unwindstack/EnhanceDlsym.h>
#include <cstdio>
#include <elf.h>
#include <inttypes.h>
#include <android/log.h>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../../common/log.h"

#define TAG "unwind-EnhanceDlsym"

namespace unwindstack {

    void EnhanceDlsym::load(const string file_name__) {

        bool          found = false;
        uintptr_t     map_base_addr;
        char          map_perm[5];
        unsigned long map_offset;
        char          map_file_name[512];

        ifstream    input("/proc/self/maps");
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

//            LOGD(TAG, "%s", aline.c_str());

            if (file_name__ == map_file_name) {
                found = true;
                break;
            }
        }

        if (!found || map_perm[0] != 'r' || map_perm[3] != 'p') {
            LOGE(TAG, "maps entry not found, %d, %c, %c", found, map_perm[0], map_perm[3]);
            return;
        }

        ElfMeta context;

        context.pathname  = file_name__;
        context.base_addr = map_base_addr;
        context.ehdr      = reinterpret_cast<ElfW(Ehdr) *>(map_base_addr);
        context.phdr      = reinterpret_cast<ElfW(Phdr) *>(map_base_addr + context.ehdr->e_phoff);

        //find the first load-segment with offset 0
        ElfW(Phdr) *phdr0 = GetFirstSegmentByTypeOffset(context, PT_LOAD, 0);
        if (!phdr0) {
            LOGE(TAG, "Can NOT found the first load segment. %s", file_name__.c_str());
            // TODO return true
            return;
        }

        //save load bias addr
        if (context.base_addr < phdr0->p_vaddr) {
            LOGE(TAG, "base_addr < phdr0->p_vaddr");
            return;
        }
        context.bias_addr = context.base_addr - phdr0->p_vaddr;

        LoadFromFile(context, file_name__);

        mOpenedFile.insert({file_name__, context});
    }

    void *EnhanceDlsym::dlsym(const string &file_name__, const string &symbol__) {

        lock_guard<std::mutex> lock(mMutex);

        if (!mOpenedFile.count(file_name__)) {
            LOGD(TAG, "%s have not been loaded yet, load it now", file_name__.c_str());
            load(file_name__);
        }

        if (!mOpenedFile.count(file_name__)) {
            LOGD(TAG, "load %s failed", file_name__.c_str());
            return nullptr;
        }

        auto &context = mOpenedFile.at(file_name__);

        if (context.found_syms.count(symbol__)) {
            LOGD(TAG, "dlsym easy");
            return reinterpret_cast<void *>(context.found_syms.at(symbol__));
        }

        LOGD(TAG, "dlsym hard");
        ElfW(Sym) *symtab_end = context.symtab + context.symtab_num;
        ElfW(Sym) *symtab_idx = context.symtab;

        for (; symtab_idx < symtab_end; symtab_idx++) {

            if (context.strtab_size <= symtab_idx->st_name) {
                LOGD(TAG, "context.strtabsz = %d, symtab_idx->st_name = %d",
                     context.strtab_size, symtab_idx->st_name);
            }
            assert (context.strtab_size > symtab_idx->st_name);

            string sym_name(context.strtab + symtab_idx->st_name);
            if (sym_name == symbol__) {
                LOGD(TAG, "st_value=%llx", symtab_idx->st_value);
                uintptr_t found_sym_addr = symtab_idx->st_value + context.bias_addr;
                context.found_syms.insert({symbol__, found_sym_addr});
                return reinterpret_cast<void *>(found_sym_addr);
            }
        }
        return nullptr;
    }

    ElfW(Phdr) *EnhanceDlsym::GetFirstSegmentByTypeOffset(ElfMeta &meta__, ElfW(Word) type,
                                                          ElfW(Off) offset) {
        ElfW(Phdr) *phdr;

        for (phdr = meta__.phdr; phdr < meta__.phdr + meta__.ehdr->e_phnum; phdr++) {
            if (phdr->p_type == type && phdr->p_offset == offset) {
                return phdr;
            }
        }
        return nullptr;
    }

    void EnhanceDlsym::LoadFromFile(ElfMeta &meta__, const string &file_name__) {
        int   fd        = open(file_name__.c_str(), O_RDONLY | O_CLOEXEC);
        off_t elf_size  = lseek(fd, 0, SEEK_END);
        ElfW(Ehdr) *elf = static_cast<ElfW(Ehdr) *>(mmap(0, elf_size, PROT_READ, MAP_SHARED, fd,
                                                         0));
        // TODO check elf header

        ElfW(Shdr) *shdr = reinterpret_cast<ElfW(Shdr) *>(((uintptr_t) elf) + elf->e_shoff);

        ElfW(Shdr) *shdr_end = reinterpret_cast<ElfW(Shdr) *>(((uintptr_t) shdr) +
                                                              elf->e_shentsize * elf->e_shnum);

        ElfW(Shdr) *shdr_shstrtab = reinterpret_cast<ElfW(Shdr) *>((uintptr_t) shdr +
                                                                   elf->e_shstrndx *
                                                                   elf->e_shentsize);

        auto shstr = reinterpret_cast<const char *>(((uintptr_t) elf) +
                                                    shdr_shstrtab->sh_offset);

        short flag = 0b11;
        do {
            shdr_end--;

            switch (shdr_end->sh_type) {

                case SHT_SYMTAB:
                    LOGD(TAG, "SHT_SYMTAB");
                    meta__.symtab = static_cast<ElfW(Sym) *>(malloc(shdr_end->sh_size));
                    memcpy(meta__.symtab,
                           reinterpret_cast<const void *>(((uintptr_t) elf) + shdr_end->sh_offset),
                           shdr_end->sh_size);
                    meta__.symtab_num = shdr_end->sh_size / sizeof(ElfW(Sym));

                    flag &= 0b01;
                    break;

                case SHT_STRTAB:
                    LOGD(TAG, "SHT_STRTAB");

                    if (0 == strcmp(shstr + shdr_end->sh_name, ".strtab")) {
                        meta__.strtab   = static_cast<char *>(malloc(shdr_end->sh_size));
                        meta__.strtab_size = shdr_end->sh_size;
                        memcpy(meta__.strtab,
                               reinterpret_cast<const void *>(((uintptr_t) elf) +
                                                              shdr_end->sh_offset),
                               shdr_end->sh_size);

                        flag &= 0b10;
                    }

                    break;
            }

            LOGD(TAG, "flag = %d", flag);

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
        LOGD(TAG, "got symtab size = %d", meta__.symtab_num);

        munmap(elf, elf_size);
        close(fd);
    }

    EnhanceDlsym *EnhanceDlsym::INSTANCE = new (nothrow) EnhanceDlsym;
}
