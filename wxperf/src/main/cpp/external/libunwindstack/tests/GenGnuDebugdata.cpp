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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#if !defined(EM_AARCH64)
#define EM_AARCH64 183
#endif

template <typename Ehdr>
void InitEhdr(Ehdr* ehdr, uint32_t elf_class, uint32_t machine) {
  memset(ehdr, 0, sizeof(Ehdr));
  memcpy(&ehdr->e_ident[0], ELFMAG, SELFMAG);
  ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
  ehdr->e_ident[EI_VERSION] = EV_CURRENT;
  ehdr->e_ident[EI_OSABI] = ELFOSABI_SYSV;
  ehdr->e_ident[EI_CLASS] = elf_class;
  ehdr->e_type = ET_DYN;
  ehdr->e_machine = machine;
  ehdr->e_version = EV_CURRENT;
  ehdr->e_ehsize = sizeof(Ehdr);
}

template <typename Ehdr, typename Shdr>
void GenElf(Ehdr* ehdr, int fd) {
  uint64_t offset = sizeof(Ehdr);
  ehdr->e_shoff = offset;
  ehdr->e_shnum = 3;
  ehdr->e_shentsize = sizeof(Shdr);
  ehdr->e_shstrndx = 2;
  TEMP_FAILURE_RETRY(write(fd, ehdr, sizeof(Ehdr)));

  Shdr shdr;
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_name = 0;
  shdr.sh_type = SHT_NULL;
  TEMP_FAILURE_RETRY(write(fd, &shdr, sizeof(Shdr)));
  offset += ehdr->e_shentsize;

  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_PROGBITS;
  shdr.sh_name = 11;
  shdr.sh_addr = 0x5000;
  shdr.sh_offset = 0x5000;
  shdr.sh_entsize = 0x100;
  shdr.sh_size = 0x800;
  TEMP_FAILURE_RETRY(write(fd, &shdr, sizeof(Shdr)));
  offset += ehdr->e_shentsize;

  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_STRTAB;
  shdr.sh_name = 1;
  shdr.sh_offset = 0x200;
  shdr.sh_size = 24;
  TEMP_FAILURE_RETRY(write(fd, &shdr, sizeof(Shdr)));

  // Write out the name entries information.
  lseek(fd, 0x200, SEEK_SET);
  std::string name;
  TEMP_FAILURE_RETRY(write(fd, name.data(), name.size() + 1));
  name = ".shstrtab";
  TEMP_FAILURE_RETRY(write(fd, name.data(), name.size() + 1));
  name = ".debug_frame";
  TEMP_FAILURE_RETRY(write(fd, name.data(), name.size() + 1));
}

int main() {
  int elf32_fd = TEMP_FAILURE_RETRY(open("elf32", O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644));
  if (elf32_fd == -1) {
    printf("Failed to create elf32: %s\n", strerror(errno));
    return 1;
  }

  int elf64_fd = TEMP_FAILURE_RETRY(open("elf64", O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644));
  if (elf64_fd == -1) {
    printf("Failed to create elf64: %s\n", strerror(errno));
    return 1;
  }

  Elf32_Ehdr ehdr32;
  InitEhdr<Elf32_Ehdr>(&ehdr32, ELFCLASS32, EM_ARM);
  GenElf<Elf32_Ehdr, Elf32_Shdr>(&ehdr32, elf32_fd);
  close(elf32_fd);

  Elf64_Ehdr ehdr64;
  InitEhdr<Elf64_Ehdr>(&ehdr64, ELFCLASS64, EM_AARCH64);
  GenElf<Elf64_Ehdr, Elf64_Shdr>(&ehdr64, elf64_fd);
  close(elf64_fd);
}
