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

#ifndef __TOM_ELF_HOOK_H__
#define __TOM_ELF_HOOK_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include "elf_def.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name;
    Elf_Addr base;
    Elf_Addr bias;
    uint32_t privilege;

    Elf_Ehdr* ehdr;
    Elf_Phdr* phdr;
    Elf_Shdr* shdr;

    Elf_Dyn* dyn;
    size_t dyn_count;

    Elf_Sym* sym;
    size_t sym_count;

    const char* symstr;

    Elf_Addr plt_rel;
    size_t plt_rel_count;

    Elf_Addr rel;
    size_t rel_count;

    size_t nbucket;
    unsigned *bucket;
    size_t nchain;
    unsigned *chain;

    int use_rela;
} loaded_soinfo;

extern loaded_soinfo *elfhook_open(const char *so_path);

extern int elfhook_replace(const loaded_soinfo *soinfo, const char *func_name, void *new_func,
                           void **original_func);

extern void elfhook_close(loaded_soinfo *soinfo);

#ifdef __cplusplus
}
#endif

#endif
