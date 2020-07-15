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

#include <stdint.h>
#include <sys/mman.h>

#include <memory>
#include <vector>

#include <unwindstack/Elf.h>
#include <unwindstack/JitDebug.h>
#include <unwindstack/Maps.h>

#include "MemoryRange.h"

// This implements the JIT Compilation Interface.
// See https://sourceware.org/gdb/onlinedocs/gdb/JIT-Interface.html

namespace unwindstack {

struct JITCodeEntry32Pack {
  uint32_t next;
  uint32_t prev;
  uint32_t symfile_addr;
  uint64_t symfile_size;
} __attribute__((packed));

struct JITCodeEntry32Pad {
  uint32_t next;
  uint32_t prev;
  uint32_t symfile_addr;
  uint32_t pad;
  uint64_t symfile_size;
};

struct JITCodeEntry64 {
  uint64_t next;
  uint64_t prev;
  uint64_t symfile_addr;
  uint64_t symfile_size;
};

struct JITDescriptorHeader {
  uint32_t version;
  uint32_t action_flag;
};

struct JITDescriptor32 {
  JITDescriptorHeader header;
  uint32_t relevant_entry;
  uint32_t first_entry;
};

struct JITDescriptor64 {
  JITDescriptorHeader header;
  uint64_t relevant_entry;
  uint64_t first_entry;
};

JitDebug::JitDebug(std::shared_ptr<Memory>& memory) : Global(memory) {}

JitDebug::JitDebug(std::shared_ptr<Memory>& memory, std::vector<std::string>& search_libs)
    : Global(memory, search_libs) {}

JitDebug::~JitDebug() {
  for (auto* elf : elf_list_) {
    delete elf;
  }
}

uint64_t JitDebug::ReadDescriptor32(uint64_t addr) {
  JITDescriptor32 desc;
  if (!memory_->ReadFully(addr, &desc, sizeof(desc))) {
    return 0;
  }

  if (desc.header.version != 1 || desc.first_entry == 0) {
    // Either unknown version, or no jit entries.
    return 0;
  }

  return desc.first_entry;
}

uint64_t JitDebug::ReadDescriptor64(uint64_t addr) {
  JITDescriptor64 desc;
  if (!memory_->ReadFully(addr, &desc, sizeof(desc))) {
    return 0;
  }

  if (desc.header.version != 1 || desc.first_entry == 0) {
    // Either unknown version, or no jit entries.
    return 0;
  }

  return desc.first_entry;
}

uint64_t JitDebug::ReadEntry32Pack(uint64_t* start, uint64_t* size) {
  JITCodeEntry32Pack code;
  if (!memory_->ReadFully(entry_addr_, &code, sizeof(code))) {
    return 0;
  }

  *start = code.symfile_addr;
  *size = code.symfile_size;
  return code.next;
}

uint64_t JitDebug::ReadEntry32Pad(uint64_t* start, uint64_t* size) {
  JITCodeEntry32Pad code;
  if (!memory_->ReadFully(entry_addr_, &code, sizeof(code))) {
    return 0;
  }

  *start = code.symfile_addr;
  *size = code.symfile_size;
  return code.next;
}

uint64_t JitDebug::ReadEntry64(uint64_t* start, uint64_t* size) {
  JITCodeEntry64 code;
  if (!memory_->ReadFully(entry_addr_, &code, sizeof(code))) {
    return 0;
  }

  *start = code.symfile_addr;
  *size = code.symfile_size;
  return code.next;
}

void JitDebug::ProcessArch() {
  switch (arch()) {
    case ARCH_X86:
      read_descriptor_func_ = &JitDebug::ReadDescriptor32;
      read_entry_func_ = &JitDebug::ReadEntry32Pack;
      break;

    case ARCH_ARM:
    case ARCH_MIPS:
      read_descriptor_func_ = &JitDebug::ReadDescriptor32;
      read_entry_func_ = &JitDebug::ReadEntry32Pad;
      break;

    case ARCH_ARM64:
    case ARCH_X86_64:
    case ARCH_MIPS64:
      read_descriptor_func_ = &JitDebug::ReadDescriptor64;
      read_entry_func_ = &JitDebug::ReadEntry64;
      break;
    case ARCH_UNKNOWN:
      abort();
  }
}

bool JitDebug::ReadVariableData(uint64_t ptr) {
  entry_addr_ = (this->*read_descriptor_func_)(ptr);
  return entry_addr_ != 0;
}

void JitDebug::Init(Maps* maps) {
  if (initialized_) {
    return;
  }
  // Regardless of what happens below, consider the init finished.
  initialized_ = true;

  FindAndReadVariable(maps, "__jit_debug_descriptor");
}

Elf* JitDebug::GetElf(Maps* maps, uint64_t pc) {
  // Use a single lock, this object should be used so infrequently that
  // a fine grain lock is unnecessary.
  std::lock_guard<std::mutex> guard(lock_);
  if (!initialized_) {
    Init(maps);
  }

  // Search the existing elf object first.
  for (Elf* elf : elf_list_) {
    if (elf->IsValidPc(pc)) {
      return elf;
    }
  }

  while (entry_addr_ != 0) {
    uint64_t start;
    uint64_t size;
    entry_addr_ = (this->*read_entry_func_)(&start, &size);

    Elf* elf = new Elf(new MemoryRange(memory_, start, size, 0));
    elf->Init();
    if (!elf->valid()) {
      // The data is not formatted in a way we understand, do not attempt
      // to process any other entries.
      entry_addr_ = 0;
      delete elf;
      return nullptr;
    }
    elf_list_.push_back(elf);

    if (elf->IsValidPc(pc)) {
      return elf;
    }
  }
  return nullptr;
}

}  // namespace unwindstack
