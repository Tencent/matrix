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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <unwindstack/Elf.h>
#include <unwindstack/Log.h>
#include <unwindstack/Memory.h>

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    printf("Usage: unwind_symbols <ELF_FILE> [<FUNC_ADDRESS>]\n");
    printf("  Dump all function symbols in ELF_FILE. If FUNC_ADDRESS is\n");
    printf("  specified, then get the function at that address.\n");
    printf("  FUNC_ADDRESS must be a hex number.\n");
    return 1;
  }

  struct stat st;
  if (stat(argv[1], &st) == -1) {
    printf("Cannot stat %s: %s\n", argv[1], strerror(errno));
    return 1;
  }
  if (!S_ISREG(st.st_mode)) {
    printf("%s is not a regular file.\n", argv[1]);
    return 1;
  }

  uint64_t func_addr;
  if (argc == 3) {
    char* name;
    func_addr = strtoull(argv[2], &name, 16);
    if (*name != '\0') {
      printf("%s is not a hex number.\n", argv[2]);
      return 1;
    }
  }

  // Send all log messages to stdout.
  unwindstack::log_to_stdout(true);

  unwindstack::MemoryFileAtOffset* memory = new unwindstack::MemoryFileAtOffset;
  if (!memory->Init(argv[1], 0)) {
    printf("Failed to init\n");
    return 1;
  }

  unwindstack::Elf elf(memory);
  if (!elf.Init(true) || !elf.valid()) {
    printf("%s is not a valid elf file.\n", argv[1]);
    return 1;
  }

  std::string soname;
  if (elf.GetSoname(&soname)) {
    printf("Soname: %s\n\n", soname.c_str());
  }

  switch (elf.machine_type()) {
    case EM_ARM:
      printf("ABI: arm\n");
      break;
    case EM_AARCH64:
      printf("ABI: arm64\n");
      break;
    case EM_386:
      printf("ABI: x86\n");
      break;
    case EM_X86_64:
      printf("ABI: x86_64\n");
      break;
    default:
      printf("ABI: unknown\n");
      return 1;
  }

  std::string name;
  uint64_t load_bias = elf.GetLoadBias();
  if (argc == 3) {
    std::string cur_name;
    uint64_t func_offset;
    if (!elf.GetFunctionName(func_addr, &cur_name, &func_offset)) {
      printf("No known function at 0x%" PRIx64 "\n", func_addr);
      return 1;
    }
    printf("<0x%" PRIx64 ">", func_addr - func_offset);
    if (func_offset != 0) {
      printf("+%" PRId64, func_offset);
    }
    printf(": %s\n", cur_name.c_str());
    return 0;
  }

  // This is a crude way to get the symbols in order.
  for (const auto& entry : elf.interface()->pt_loads()) {
    uint64_t start = entry.second.offset + load_bias;
    uint64_t end = entry.second.table_size + load_bias;
    for (uint64_t addr = start; addr < end; addr += 4) {
      std::string cur_name;
      uint64_t func_offset;
      if (elf.GetFunctionName(addr, &cur_name, &func_offset)) {
        if (cur_name != name) {
          printf("<0x%" PRIx64 "> Function: %s\n", addr - func_offset, cur_name.c_str());
        }
        name = cur_name;
      }
    }
  }

  return 0;
}
