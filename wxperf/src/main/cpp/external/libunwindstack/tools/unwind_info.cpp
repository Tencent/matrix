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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <unwindstack/DwarfSection.h>
#include <unwindstack/DwarfStructs.h>
#include <unwindstack/Elf.h>
#include <unwindstack/ElfInterface.h>
#include <unwindstack/Log.h>

#include "ArmExidx.h"
#include "ElfInterfaceArm.h"

namespace unwindstack {

void DumpArm(Elf* elf, ElfInterfaceArm* interface) {
  if (interface == nullptr) {
    printf("No ARM Unwind Information.\n\n");
    return;
  }

  printf("ARM Unwind Information:\n");
  uint64_t load_bias = elf->GetLoadBias();
  for (const auto& entry : interface->pt_loads()) {
    printf(" PC Range 0x%" PRIx64 " - 0x%" PRIx64 "\n", entry.second.offset + load_bias,
           entry.second.offset + entry.second.table_size + load_bias);
    for (auto pc : *interface) {
      std::string name;
      printf("  PC 0x%" PRIx64, pc + load_bias);
      uint64_t func_offset;
      if (elf->GetFunctionName(pc + load_bias, &name, &func_offset) && !name.empty()) {
        printf(" <%s>", name.c_str());
      }
      printf("\n");
      uint64_t entry;
      if (!interface->FindEntry(pc, &entry)) {
        printf("    Cannot find entry for address.\n");
        continue;
      }
      ArmExidx arm(nullptr, interface->memory(), nullptr);
      arm.set_log(ARM_LOG_FULL);
      arm.set_log_skip_execution(true);
      arm.set_log_indent(2);
      if (!arm.ExtractEntryData(entry)) {
        if (arm.status() != ARM_STATUS_NO_UNWIND) {
          printf("    Error trying to extract data.\n");
        }
        continue;
      }
      if (arm.data()->size() > 0) {
        if (!arm.Eval() && arm.status() != ARM_STATUS_NO_UNWIND) {
          printf("      Error trying to evaluate dwarf data.\n");
        }
      }
    }
  }
  printf("\n");
}

void DumpDwarfSection(Elf* elf, DwarfSection* section, uint64_t) {
  for (const DwarfFde* fde : *section) {
    // Sometimes there are entries that have empty length, skip those since
    // they don't contain any interesting information.
    if (fde == nullptr || fde->pc_start == fde->pc_end) {
      continue;
    }
    printf("\n  PC 0x%" PRIx64 "-0x%" PRIx64, fde->pc_start, fde->pc_end);
    std::string name;
    uint64_t func_offset;
    if (elf->GetFunctionName(fde->pc_start, &name, &func_offset) && !name.empty()) {
      printf(" <%s>", name.c_str());
    }
    printf("\n");
    if (!section->Log(2, UINT64_MAX, fde)) {
      printf("Failed to process cfa information for entry at 0x%" PRIx64 "\n", fde->pc_start);
    }
  }
}

int GetElfInfo(const char* file, uint64_t offset) {
  // Send all log messages to stdout.
  log_to_stdout(true);

  MemoryFileAtOffset* memory = new MemoryFileAtOffset;
  if (!memory->Init(file, offset)) {
    // Initializatation failed.
    printf("Failed to init\n");
    return 1;
  }

  Elf elf(memory);
  if (!elf.Init() || !elf.valid()) {
    printf("%s is not a valid elf file.\n", file);
    return 1;
  }

  std::string soname(elf.GetSoname());
  if (!soname.empty()) {
    printf("Soname: %s\n", soname.c_str());
  }

  std::string build_id = elf.GetBuildID();
  if (!build_id.empty()) {
    printf("Build ID: ");
    for (size_t i = 0; i < build_id.size(); ++i) {
      printf("%02hhx", build_id[i]);
    }
    printf("\n");
  }

  ElfInterface* interface = elf.interface();
  if (elf.machine_type() == EM_ARM) {
    DumpArm(&elf, reinterpret_cast<ElfInterfaceArm*>(interface));
    printf("\n");
  }

  if (interface->eh_frame() != nullptr) {
    printf("eh_frame information:\n");
    DumpDwarfSection(&elf, interface->eh_frame(), elf.GetLoadBias());
    printf("\n");
  } else {
    printf("\nno eh_frame information\n");
  }

  if (interface->debug_frame() != nullptr) {
    printf("\ndebug_frame information:\n");
    DumpDwarfSection(&elf, interface->debug_frame(), elf.GetLoadBias());
    printf("\n");
  } else {
    printf("\nno debug_frame information\n");
  }

  // If there is a gnu_debugdata interface, dump the information for that.
  ElfInterface* gnu_debugdata_interface = elf.gnu_debugdata_interface();
  if (gnu_debugdata_interface != nullptr) {
    if (gnu_debugdata_interface->eh_frame() != nullptr) {
      printf("\ngnu_debugdata (eh_frame):\n");
      DumpDwarfSection(&elf, gnu_debugdata_interface->eh_frame(), 0);
      printf("\n");
    }
    if (gnu_debugdata_interface->debug_frame() != nullptr) {
      printf("\ngnu_debugdata (debug_frame):\n");
      DumpDwarfSection(&elf, gnu_debugdata_interface->debug_frame(), 0);
      printf("\n");
    }
  } else {
    printf("\nno valid gnu_debugdata information\n");
  }

  return 0;
}

}  // namespace unwindstack

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    printf("Usage: unwind_info ELF_FILE [OFFSET]\n");
    printf("  ELF_FILE\n");
    printf("    The path to an elf file.\n");
    printf("  OFFSET\n");
    printf("    Use the offset into the ELF file as the beginning of the elf.\n");
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

  uint64_t offset = 0;
  if (argc == 3) {
    char* end;
    offset = strtoull(argv[2], &end, 16);
    if (*end != '\0') {
      printf("Malformed OFFSET value: %s\n", argv[2]);
      return 1;
    }
  }

  return unwindstack::GetElfInfo(argv[1], offset);
}
