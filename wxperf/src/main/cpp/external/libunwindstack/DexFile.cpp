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

#include <android-base/unique_fd.h>

#include <dex/code_item_accessors-inl.h>
#include <dex/compact_dex_file.h>
#include <dex/dex_file-inl.h>
#include <dex/dex_file_loader.h>
#include <dex/standard_dex_file.h>

#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>

#include "DexFile.h"

namespace unwindstack {

DexFile* DexFile::Create(uint64_t dex_file_offset_in_memory, Memory* memory, MapInfo* info) {
  if (!info->name.empty()) {
    std::unique_ptr<DexFileFromFile> dex_file(new DexFileFromFile);
    if (dex_file->Open(dex_file_offset_in_memory - info->start + info->offset, info->name)) {
      return dex_file.release();
    }
  }

  std::unique_ptr<DexFileFromMemory> dex_file(new DexFileFromMemory);
  if (dex_file->Open(dex_file_offset_in_memory, memory)) {
    return dex_file.release();
  }
  return nullptr;
}

DexFileFromFile::~DexFileFromFile() {
  if (size_ != 0) {
    munmap(mapped_memory_, size_);
  }
}

bool DexFile::GetMethodInformation(uint64_t dex_offset, std::string* method_name,
                                   uint64_t* method_offset) {
  if (dex_file_ == nullptr) {
    return false;
  }

  if (!dex_file_->IsInDataSection(dex_file_->Begin() + dex_offset)) {
    return false;  // The DEX offset is not within the bytecode of this dex file.
  }

  for (uint32_t i = 0; i < dex_file_->NumClassDefs(); ++i) {
    const art::DexFile::ClassDef& class_def = dex_file_->GetClassDef(i);
    const uint8_t* class_data = dex_file_->GetClassData(class_def);
    if (class_data == nullptr) {
      continue;
    }
    for (art::ClassDataItemIterator it(*dex_file_.get(), class_data); it.HasNext(); it.Next()) {
      if (!it.IsAtMethod()) {
        continue;
      }
      const art::DexFile::CodeItem* code_item = it.GetMethodCodeItem();
      if (code_item == nullptr) {
        continue;
      }
      art::CodeItemInstructionAccessor code(*dex_file_.get(), code_item);
      if (!code.HasCodeItem()) {
        continue;
      }

      uint64_t offset = reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file_->Begin();
      size_t size = code.InsnsSizeInCodeUnits() * sizeof(uint16_t);
      if (offset <= dex_offset && dex_offset < offset + size) {
        *method_name = dex_file_->PrettyMethod(it.GetMemberIndex(), false);
        *method_offset = dex_offset - offset;
        return true;
      }
    }
  }
  return false;
}

bool DexFileFromFile::Open(uint64_t dex_file_offset_in_file, const std::string& file) {
  android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(file.c_str(), O_RDONLY | O_CLOEXEC)));
  if (fd == -1) {
    return false;
  }
  struct stat buf;
  if (fstat(fd, &buf) == -1) {
    return false;
  }
  uint64_t length;
  if (buf.st_size < 0 ||
      __builtin_add_overflow(dex_file_offset_in_file, sizeof(art::DexFile::Header), &length) ||
      static_cast<uint64_t>(buf.st_size) < length) {
    return false;
  }

  mapped_memory_ = mmap(nullptr, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped_memory_ == MAP_FAILED) {
    return false;
  }
  size_ = buf.st_size;

  uint8_t* memory = reinterpret_cast<uint8_t*>(mapped_memory_);

  art::DexFile::Header* header =
      reinterpret_cast<art::DexFile::Header*>(&memory[dex_file_offset_in_file]);
  if (!art::StandardDexFile::IsMagicValid(header->magic_) &&
      !art::CompactDexFile::IsMagicValid(header->magic_)) {
    return false;
  }

  if (__builtin_add_overflow(dex_file_offset_in_file, header->file_size_, &length) ||
      static_cast<uint64_t>(buf.st_size) < length) {
    return false;
  }

  art::DexFileLoader loader;
  std::string error_msg;
  auto dex = loader.Open(&memory[dex_file_offset_in_file], header->file_size_, "", 0, nullptr,
                         false, false, &error_msg);
  dex_file_.reset(dex.release());
  return dex_file_ != nullptr;
}

bool DexFileFromMemory::Open(uint64_t dex_file_offset_in_memory, Memory* memory) {
  memory_.resize(sizeof(art::DexFile::Header));
  if (!memory->ReadFully(dex_file_offset_in_memory, memory_.data(), memory_.size())) {
    return false;
  }

  art::DexFile::Header* header = reinterpret_cast<art::DexFile::Header*>(memory_.data());
  uint32_t file_size = header->file_size_;
  if (art::CompactDexFile::IsMagicValid(header->magic_)) {
    // Compact dex file store data section separately so that it can be shared.
    // Therefore we need to extend the read memory range to include it.
    // TODO: This might be wasteful as we might read data in between as well.
    //       In practice, this should be fine, as such sharing only happens on disk.
    uint32_t computed_file_size;
    if (__builtin_add_overflow(header->data_off_, header->data_size_, &computed_file_size)) {
      return false;
    }
    if (computed_file_size > file_size) {
      file_size = computed_file_size;
    }
  } else if (!art::StandardDexFile::IsMagicValid(header->magic_)) {
    return false;
  }

  memory_.resize(file_size);
  if (!memory->ReadFully(dex_file_offset_in_memory, memory_.data(), memory_.size())) {
    return false;
  }

  header = reinterpret_cast<art::DexFile::Header*>(memory_.data());

  art::DexFileLoader loader;
  std::string error_msg;
  auto dex =
      loader.Open(memory_.data(), header->file_size_, "", 0, nullptr, false, false, &error_msg);
  dex_file_.reset(dex.release());
  return dex_file_ != nullptr;
}

}  // namespace unwindstack
