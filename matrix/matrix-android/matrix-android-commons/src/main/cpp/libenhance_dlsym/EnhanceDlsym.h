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

#ifndef LIBMATRIX_JNI_ENHANCEDLSYM_H
#define LIBMATRIX_JNI_ENHANCEDLSYM_H

#include <map>
#include <string>
#include <link.h>
#include <mutex>

namespace enhance {
    void* dlopen(const char* __file_name, int __flag);
    int dlclose(void* __handle);
    void* dlsym(void* __handle, const char* __symbol);
    size_t dlsizeof(void *__addr);

    struct DlInfo {

        DlInfo() {}
        ~DlInfo() {
            if (strtab) {
                free(strtab);
            }
            if (symtab) {
                free(symtab);
            }
        }

        std::string pathname;

        ElfW(Addr)  base_addr;
        ElfW(Addr)  bias_addr;

        ElfW(Ehdr) *ehdr; // pointing to loaded mem
        ElfW(Phdr) *phdr; // pointing to loaded mem

        char *strtab; // strtab
        ElfW(Word) strtab_size; // size in bytes

        ElfW(Sym)  *symtab;
        ElfW(Word) symtab_num;

    };
}

#endif //LIBMATRIX_JNI_ENHANCEDLSYM_H
