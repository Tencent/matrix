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


#ifndef __TOM_ELF_DEF_H__
#define __TOM_ELF_DEF_H__

#include <linux/elf.h>

#ifdef __LP64__

typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Dyn Elf_Dyn;
typedef Elf64_Sym Elf_Sym;
typedef Elf64_Word Elf_Word;
typedef Elf64_Addr Elf_Addr;
typedef Elf64_Rel Elf_Rel;
typedef Elf64_Rela Elf_Rela;

#define ELF_R_SYM(x) ELF64_R_SYM(x)
#define ELF_R_TYPE(x) ELF64_R_TYPE(x)

#else

typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
typedef Elf32_Shdr Elf_Shdr;
typedef Elf32_Dyn Elf_Dyn;
typedef Elf32_Sym Elf_Sym;
typedef Elf32_Word Elf_Word;
typedef Elf32_Addr Elf_Addr;
typedef Elf32_Rel Elf_Rel;
typedef Elf32_Rela Elf_Rela;

#define ELF_R_SYM(x) ELF32_R_SYM(x)
#define ELF_R_TYPE(x) ELF32_R_TYPE(x)

#endif

#define DT_GNU_HASH 0x6ffffef5

#endif
