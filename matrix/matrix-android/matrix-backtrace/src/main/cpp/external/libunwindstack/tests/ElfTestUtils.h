/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBUNWINDSTACK_TESTS_ELF_TEST_UTILS_H
#define _LIBUNWINDSTACK_TESTS_ELF_TEST_UTILS_H

#include <functional>
#include <string>

namespace unwindstack {

typedef std::function<void(uint64_t, const void*, size_t)> TestCopyFuncType;

template <typename Ehdr>
void TestInitEhdr(Ehdr* ehdr, uint32_t elf_class, uint32_t machine_type);

template <typename Ehdr, typename Shdr>
void TestInitGnuDebugdata(uint32_t elf_class, uint32_t machine_type, bool init_gnu_debudata,
                          TestCopyFuncType copy_func);

std::string TestGetFileDirectory();

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_TESTS_ELF_TEST_UTILS_H
