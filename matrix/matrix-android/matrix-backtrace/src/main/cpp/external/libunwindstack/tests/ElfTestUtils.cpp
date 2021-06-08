/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ElfTestUtils.h"

namespace unwindstack {

template <typename Ehdr>
void TestInitEhdr(Ehdr* ehdr, uint32_t elf_class, uint32_t machine_type) {
  memset(ehdr, 0, sizeof(Ehdr));
  memcpy(&ehdr->e_ident[0], ELFMAG, SELFMAG);
  ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
  ehdr->e_ident[EI_VERSION] = EV_CURRENT;
  ehdr->e_ident[EI_OSABI] = ELFOSABI_SYSV;
  ehdr->e_ident[EI_CLASS] = elf_class;
  ehdr->e_type = ET_DYN;
  ehdr->e_machine = machine_type;
  ehdr->e_version = EV_CURRENT;
  ehdr->e_ehsize = sizeof(Ehdr);
}

std::string TestGetFileDirectory() {
  std::string exec(testing::internal::GetArgvs()[0]);
  auto const value = exec.find_last_of('/');
  if (value == std::string::npos) {
    return "tests/files/";
  }
  return exec.substr(0, value + 1) + "tests/files/";
}

template <typename Ehdr, typename Shdr>
void TestInitGnuDebugdata(uint32_t elf_class, uint32_t machine, bool init_gnu_debugdata,
                          TestCopyFuncType copy_func) {
  Ehdr ehdr;

  TestInitEhdr(&ehdr, elf_class, machine);

  uint64_t offset = sizeof(Ehdr);
  ehdr.e_shoff = offset;
  ehdr.e_shnum = 3;
  ehdr.e_shentsize = sizeof(Shdr);
  ehdr.e_shstrndx = 2;
  copy_func(0, &ehdr, sizeof(ehdr));

  Shdr shdr;
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_NULL;
  copy_func(offset, &shdr, sizeof(shdr));
  offset += ehdr.e_shentsize;

  // Skip this header, it will contain the gnu_debugdata information.
  uint64_t gnu_offset = offset;
  offset += ehdr.e_shentsize;

  uint64_t symtab_offset = sizeof(ehdr) + ehdr.e_shnum * ehdr.e_shentsize;
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_name = 1;
  shdr.sh_type = SHT_STRTAB;
  shdr.sh_offset = symtab_offset;
  shdr.sh_size = 0x100;
  copy_func(offset, &shdr, sizeof(shdr));

  char value = '\0';
  uint64_t symname_offset = symtab_offset;
  copy_func(symname_offset, &value, 1);
  symname_offset++;
  std::string name(".shstrtab");
  copy_func(symname_offset, name.c_str(), name.size() + 1);
  symname_offset += name.size() + 1;
  name = ".gnu_debugdata";
  copy_func(symname_offset, name.c_str(), name.size() + 1);

  ssize_t bytes = 0x100;
  offset = symtab_offset + 0x100;
  if (init_gnu_debugdata) {
    // Read in the compressed elf data and copy it in.
    name = TestGetFileDirectory();
    if (elf_class == ELFCLASS32) {
      name += "elf32.xz";
    } else {
      name += "elf64.xz";
    }
    int fd = TEMP_FAILURE_RETRY(open(name.c_str(), O_RDONLY));
    ASSERT_NE(-1, fd) << "Cannot open " + name;
    // Assumes the file is less than 1024 bytes.
    std::vector<uint8_t> buf(1024);
    bytes = TEMP_FAILURE_RETRY(read(fd, buf.data(), buf.size()));
    ASSERT_GT(bytes, 0);
    // Make sure the file isn't too big.
    ASSERT_NE(static_cast<size_t>(bytes), buf.size())
        << "File " + name + " is too big, increase buffer size.";
    close(fd);
    buf.resize(bytes);
    copy_func(offset, buf.data(), buf.size());
  }

  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_name = symname_offset - symtab_offset;
  shdr.sh_addr = offset;
  shdr.sh_offset = offset;
  shdr.sh_size = bytes;
  copy_func(gnu_offset, &shdr, sizeof(shdr));
}

template void TestInitEhdr<Elf32_Ehdr>(Elf32_Ehdr*, uint32_t, uint32_t);
template void TestInitEhdr<Elf64_Ehdr>(Elf64_Ehdr*, uint32_t, uint32_t);

template void TestInitGnuDebugdata<Elf32_Ehdr, Elf32_Shdr>(uint32_t, uint32_t, bool,
                                                           TestCopyFuncType);
template void TestInitGnuDebugdata<Elf64_Ehdr, Elf64_Shdr>(uint32_t, uint32_t, bool,
                                                           TestCopyFuncType);

}  // namespace unwindstack
