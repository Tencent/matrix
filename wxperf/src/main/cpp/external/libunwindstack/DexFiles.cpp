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

#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include <unwindstack/DexFiles.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

#include "DexFile.h"

namespace unwindstack {

struct DEXFileEntry32 {
  uint32_t next;
  uint32_t prev;
  uint32_t dex_file;
};

struct DEXFileEntry64 {
  uint64_t next;
  uint64_t prev;
  uint64_t dex_file;
};

DexFiles::DexFiles(std::shared_ptr<Memory>& memory) : Global(memory) {}

DexFiles::DexFiles(std::shared_ptr<Memory>& memory, std::vector<std::string>& search_libs)
    : Global(memory, search_libs) {}

DexFiles::~DexFiles() {}

void DexFiles::ProcessArch() {
  switch (arch()) {
    case ARCH_ARM:
    case ARCH_MIPS:
    case ARCH_X86:
      read_entry_ptr_func_ = &DexFiles::ReadEntryPtr32;
      read_entry_func_ = &DexFiles::ReadEntry32;
      break;

    case ARCH_ARM64:
    case ARCH_MIPS64:
    case ARCH_X86_64:
      read_entry_ptr_func_ = &DexFiles::ReadEntryPtr64;
      read_entry_func_ = &DexFiles::ReadEntry64;
      break;

    case ARCH_UNKNOWN:
      abort();
  }
}

uint64_t DexFiles::ReadEntryPtr32(uint64_t addr) {
  uint32_t entry;
  const uint32_t field_offset = 12;  // offset of first_entry_ in the descriptor struct.
  if (!memory_->ReadFully(addr + field_offset, &entry, sizeof(entry))) {
    return 0;
  }
  return entry;
}

uint64_t DexFiles::ReadEntryPtr64(uint64_t addr) {
  uint64_t entry;
  const uint32_t field_offset = 16;  // offset of first_entry_ in the descriptor struct.
  if (!memory_->ReadFully(addr + field_offset, &entry, sizeof(entry))) {
    return 0;
  }
  return entry;
}

bool DexFiles::ReadEntry32() {
  DEXFileEntry32 entry;
  if (!memory_->ReadFully(entry_addr_, &entry, sizeof(entry)) || entry.dex_file == 0) {
    entry_addr_ = 0;
    return false;
  }

  addrs_.push_back(entry.dex_file);
  entry_addr_ = entry.next;
  return true;
}

bool DexFiles::ReadEntry64() {
  DEXFileEntry64 entry;
  if (!memory_->ReadFully(entry_addr_, &entry, sizeof(entry)) || entry.dex_file == 0) {
    entry_addr_ = 0;
    return false;
  }

  addrs_.push_back(entry.dex_file);
  entry_addr_ = entry.next;
  return true;
}

bool DexFiles::ReadVariableData(uint64_t ptr_offset) {
  entry_addr_ = (this->*read_entry_ptr_func_)(ptr_offset);
  return entry_addr_ != 0;
}

void DexFiles::Init(Maps* maps) {
  if (initialized_) {
    return;
  }
  initialized_ = true;
  entry_addr_ = 0;

  FindAndReadVariable(maps, "__dex_debug_descriptor");
}

DexFile* DexFiles::GetDexFile(uint64_t dex_file_offset, MapInfo* info) {
  // Lock while processing the data.
  DexFile* dex_file;
  auto entry = files_.find(dex_file_offset);
  if (entry == files_.end()) {
    std::unique_ptr<DexFile> new_dex_file = DexFile::Create(dex_file_offset, memory_.get(), info);
    dex_file = new_dex_file.get();
    files_[dex_file_offset] = std::move(new_dex_file);
  } else {
    dex_file = entry->second.get();
  }
  return dex_file;
}

bool DexFiles::GetAddr(size_t index, uint64_t* addr) {
  if (index < addrs_.size()) {
    *addr = addrs_[index];
    return true;
  }
  if (entry_addr_ != 0 && (this->*read_entry_func_)()) {
    *addr = addrs_.back();
    return true;
  }
  return false;
}

void DexFiles::GetMethodInformation(Maps* maps, MapInfo* info, uint64_t dex_pc,
                                    std::string* method_name, uint64_t* method_offset) {
  std::lock_guard<std::mutex> guard(lock_);
  if (!initialized_) {
    Init(maps);
  }

  size_t index = 0;
  uint64_t addr;
  while (GetAddr(index++, &addr)) {
    if (addr < info->start || addr >= info->end) {
      continue;
    }

    DexFile* dex_file = GetDexFile(addr, info);
    if (dex_file != nullptr &&
        dex_file->GetMethodInformation(dex_pc - addr, method_name, method_offset)) {
      break;
    }
  }
}

}  // namespace unwindstack
