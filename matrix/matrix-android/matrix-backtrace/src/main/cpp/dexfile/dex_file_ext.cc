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

#include "dex_file_external.h"
#include "dex_file.h"
#include "class_accessor.h"
#include "class_accessor-inl.h"
#include "compact_dex_file.h"
#include "standard_dex_file.h"
#include "dex_file_loader.h"

#include <inttypes.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <deps/android-base/include/android-base/stringprintf.h>

namespace art {
namespace {

struct MethodCacheEntry {
  int32_t offset;  // Offset relative to the start of the dex file header.
  int32_t len;
  int32_t index;  // Method index.
};

}  // namespace
}  // namespace art

extern "C" {

// Wraps DexFile to add the caching needed by the external interface. This is
// what gets passed over as ExtDexFile*.
struct ExtDexFile {
 private:
  // Method cache for GetMethodInfoForOffset. This is populated as we iterate
  // sequentially through the class defs. MethodCacheEntry.name is only set for
  // methods returned by GetMethodInfoForOffset.
  std::map<int32_t, art::MethodCacheEntry> method_cache_;

  // Index of first class def for which method_cache_ isn't complete.
  uint32_t class_def_index_ = 0;

 public:
  std::unique_ptr<const art::DexFile> dex_file_;
  explicit ExtDexFile(std::unique_ptr<const art::DexFile>&& dex_file)
      : dex_file_(std::move(dex_file)) {}

  art::MethodCacheEntry* GetMethodCacheEntryForOffset(int64_t dex_offset) {
    // First look in the method cache.
    auto it = method_cache_.upper_bound(dex_offset);
    if (it != method_cache_.end() && dex_offset >= it->second.offset) {
      return &it->second;
    }

    for (; class_def_index_ < dex_file_->NumClassDefs(); class_def_index_++) {
      art::ClassAccessor accessor(*dex_file_, class_def_index_);

      for (const art::ClassAccessor::Method& method : accessor.GetMethods()) {
        art::CodeItemInstructionAccessor code = method.GetInstructions();
        if (!code.HasCodeItem()) {
          continue;
        }

        int32_t offset = reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file_->Begin();
        int32_t len = code.InsnsSizeInBytes();
        int32_t index = method.GetIndex();
        auto res = method_cache_.emplace(offset + len, art::MethodCacheEntry{offset, len, index});
        if (offset <= dex_offset && dex_offset < offset + len) {
          return &res.first->second;
        }
      }
    }

    return nullptr;
  }
};

int ExtDexFileOpenFromMemory(const void* addr,
                             /*inout*/ size_t* size,
                             const char* location,
                             /*out*/ const ExtDexFileString** ext_error_msg,
                             /*out*/ ExtDexFile** ext_dex_file) {
  if (*size < sizeof(art::DexFile::Header)) {
    *size = sizeof(art::DexFile::Header);
    *ext_error_msg = nullptr;
    return false;
  }

  const art::DexFile::Header* header = reinterpret_cast<const art::DexFile::Header*>(addr);
  uint32_t file_size = header->file_size_;
  if (art::CompactDexFile::IsMagicValid(header->magic_)) {
    // Compact dex files store the data section separately so that it can be shared.
    // Therefore we need to extend the read memory range to include it.
    // TODO: This might be wasteful as we might read data in between as well.
    //       In practice, this should be fine, as such sharing only happens on disk.
    uint32_t computed_file_size;
    if (__builtin_add_overflow(header->data_off_, header->data_size_, &computed_file_size)) {
      *ext_error_msg = new ExtDexFileString{
          android::base::StringPrintf("Corrupt CompactDexFile header in '%s'", location)};
      return false;
    }
    if (computed_file_size > file_size) {
      file_size = computed_file_size;
    }
  } else if (!art::StandardDexFile::IsMagicValid(header->magic_)) {
    *ext_error_msg = new ExtDexFileString{
        android::base::StringPrintf("Unrecognized dex file header in '%s'", location)};
    return false;
  }

  if (*size < file_size) {
    *size = file_size;
    *ext_error_msg = nullptr;
    return false;
  }

  std::string loc_str(location);
  art::DexFileLoader loader;
  std::string error_msg;
  std::unique_ptr<const art::DexFile> dex_file = loader.Open(static_cast<const uint8_t*>(addr),
                                                             *size,
                                                             loc_str,
                                                             header->checksum_,
                                                             /*oat_dex_file=*/nullptr,
                                                             /*verify=*/false,
                                                             /*verify_checksum=*/false,
                                                             &error_msg);
  if (dex_file == nullptr) {
    *ext_error_msg = new ExtDexFileString{std::move(error_msg)};
    return false;
  }

  *ext_dex_file = new ExtDexFile(std::move(dex_file));
  return true;
}

int ExtDexFileGetMethodInfoForOffset(ExtDexFile* ext_dex_file,
                                     int64_t dex_offset,
                                     int with_signature,
                                     /*out*/ ExtDexFileMethodInfo* method_info) {
  if (!ext_dex_file->dex_file_->IsInDataSection(ext_dex_file->dex_file_->Begin() + dex_offset)) {
    return false;  // The DEX offset is not within the bytecode of this dex file.
  }

  if (ext_dex_file->dex_file_->IsCompactDexFile()) {
    // The data section of compact dex files might be shared.
    // Check the subrange unique to this compact dex.
    const art::CompactDexFile::Header& cdex_header =
        ext_dex_file->dex_file_->AsCompactDexFile()->GetHeader();
    uint32_t begin = cdex_header.data_off_ + cdex_header.OwnedDataBegin();
    uint32_t end = cdex_header.data_off_ + cdex_header.OwnedDataEnd();
    if (dex_offset < begin || dex_offset >= end) {
      return false;  // The DEX offset is not within the bytecode of this dex file.
    }
  }

  art::MethodCacheEntry* entry = ext_dex_file->GetMethodCacheEntryForOffset(dex_offset);
  if (entry != nullptr) {
    method_info->offset = entry->offset;
    method_info->len = entry->len;
    method_info->name =
        new ExtDexFileString{ext_dex_file->dex_file_->PrettyMethod(entry->index, with_signature)};
    return true;
  }

  return false;
}

void ExtDexFileFree(ExtDexFile* ext_dex_file) { delete (ext_dex_file); }

}  // extern "C"
