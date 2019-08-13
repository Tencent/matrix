/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <unwindstack/DwarfLocation.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/DwarfSection.h>
#include <unwindstack/DwarfStructs.h>
#include <unwindstack/Elf.h>
#include <unwindstack/ElfInterface.h>
#include <unwindstack/Log.h>

#include "DwarfOp.h"

namespace unwindstack {

void PrintSignedValue(int64_t value) {
  if (value < 0) {
    printf("- %" PRId64, -value);
  } else if (value > 0) {
    printf("+ %" PRId64, value);
  }
}

void PrintExpression(Memory* memory, uint8_t class_type, uint64_t end, uint64_t length) {
  std::vector<std::string> lines;
  DwarfMemory dwarf_memory(memory);
  if (class_type == ELFCLASS32) {
    DwarfOp<uint32_t> op(&dwarf_memory, nullptr);
    op.GetLogInfo(end - length, end, &lines);
  } else {
    DwarfOp<uint64_t> op(&dwarf_memory, nullptr);
    op.GetLogInfo(end - length, end, &lines);
  }
  for (auto& line : lines) {
    printf("    %s\n", line.c_str());
  }
}

void PrintRegInformation(DwarfSection* section, Memory* memory, uint64_t pc, uint8_t class_type) {
  const DwarfFde* fde = section->GetFdeFromPc(pc);
  if (fde == nullptr) {
    printf("  No fde found.\n");
    return;
  }

  dwarf_loc_regs_t regs;
  if (!section->GetCfaLocationInfo(pc, fde, &regs)) {
    printf("  Cannot get location information.\n");
    return;
  }

  std::vector<std::pair<uint32_t, DwarfLocation>> loc_regs;
  for (auto& loc : regs) {
    loc_regs.push_back(loc);
  }
  std::sort(loc_regs.begin(), loc_regs.end(), [](auto a, auto b) {
    if (a.first == CFA_REG) {
      return true;
    } else if (b.first == CFA_REG) {
      return false;
    }
    return a.first < b.first;
  });

  for (auto& entry : loc_regs) {
    const DwarfLocation* loc = &entry.second;
    if (entry.first == CFA_REG) {
      printf("  cfa = ");
    } else {
      printf("  r%d = ", entry.first);
    }
    switch (loc->type) {
      case DWARF_LOCATION_OFFSET:
        printf("[cfa ");
        PrintSignedValue(loc->values[0]);
        printf("]\n");
        break;

      case DWARF_LOCATION_VAL_OFFSET:
        printf("cfa ");
        PrintSignedValue(loc->values[0]);
        printf("\n");
        break;

      case DWARF_LOCATION_REGISTER:
        printf("r%" PRId64 " ", loc->values[0]);
        PrintSignedValue(loc->values[1]);
        printf("\n");
        break;

      case DWARF_LOCATION_EXPRESSION: {
        printf("EXPRESSION\n");
        PrintExpression(memory, class_type, loc->values[1], loc->values[0]);
        break;
      }

      case DWARF_LOCATION_VAL_EXPRESSION: {
        printf("VAL EXPRESSION\n");
        PrintExpression(memory, class_type, loc->values[1], loc->values[0]);
        break;
      }

      case DWARF_LOCATION_UNDEFINED:
        printf("undefine\n");
        break;

      case DWARF_LOCATION_INVALID:
        printf("INVALID\n");
        break;
    }
  }
}

int GetInfo(const char* file, uint64_t pc) {
  MemoryFileAtOffset* memory = new MemoryFileAtOffset;
  if (!memory->Init(file, 0)) {
    // Initializatation failed.
    printf("Failed to init\n");
    return 1;
  }

  Elf elf(memory);
  if (!elf.Init(true) || !elf.valid()) {
    printf("%s is not a valid elf file.\n", file);
    return 1;
  }

  ElfInterface* interface = elf.interface();
  uint64_t load_bias = elf.GetLoadBias();
  if (pc < load_bias) {
    printf("PC is less than load bias.\n");
    return 1;
  }

  std::string soname;
  if (elf.GetSoname(&soname)) {
    printf("Soname: %s\n\n", soname.c_str());
  }

  printf("PC 0x%" PRIx64 ":\n", pc);

  DwarfSection* section = interface->eh_frame();
  if (section != nullptr) {
    printf("\neh_frame:\n");
    PrintRegInformation(section, memory, pc - load_bias, elf.class_type());
  } else {
    printf("\nno eh_frame information\n");
  }

  section = interface->debug_frame();
  if (section != nullptr) {
    printf("\ndebug_frame:\n");
    PrintRegInformation(section, memory, pc - load_bias, elf.class_type());
    printf("\n");
  } else {
    printf("\nno debug_frame information\n");
  }

  // If there is a gnu_debugdata interface, dump the information for that.
  ElfInterface* gnu_debugdata_interface = elf.gnu_debugdata_interface();
  if (gnu_debugdata_interface != nullptr) {
    section = gnu_debugdata_interface->eh_frame();
    if (section != nullptr) {
      printf("\ngnu_debugdata (eh_frame):\n");
      PrintRegInformation(section, gnu_debugdata_interface->memory(), pc, elf.class_type());
      printf("\n");
    } else {
      printf("\nno gnu_debugdata (eh_frame)\n");
    }

    section = gnu_debugdata_interface->debug_frame();
    if (section != nullptr) {
      printf("\ngnu_debugdata (debug_frame):\n");
      PrintRegInformation(section, gnu_debugdata_interface->memory(), pc, elf.class_type());
      printf("\n");
    } else {
      printf("\nno gnu_debugdata (debug_frame)\n");
    }
  } else {
    printf("\nno valid gnu_debugdata information\n");
  }

  return 0;
}

}  // namespace unwindstack

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("Usage: unwind_reg_info ELF_FILE PC\n");
    printf("  ELF_FILE\n");
    printf("    The path to an elf file.\n");
    printf("  PC\n");
    printf("    The pc for which the register information should be obtained.\n");
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

  uint64_t pc = 0;
  char* end;
  pc = strtoull(argv[2], &end, 16);
  if (*end != '\0') {
    printf("Malformed OFFSET value: %s\n", argv[2]);
    return 1;
  }

  return unwindstack::GetInfo(argv[1], pc);
}
