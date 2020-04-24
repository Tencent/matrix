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
#include <art_api/dex_file_support.h>

#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>

#include "DexFile.h"

namespace unwindstack {

std::unique_ptr<DexFile> DexFile::Create(uint64_t dex_file_offset_in_memory, Memory* memory,
                                         MapInfo* info) {
  if (!info->name.empty()) {
    std::unique_ptr<DexFile> dex_file =
        DexFileFromFile::Create(dex_file_offset_in_memory - info->start + info->offset, info->name);
    if (dex_file) {
      return dex_file;
    }
  }
  return DexFileFromMemory::Create(dex_file_offset_in_memory, memory, info->name);
}

bool DexFile::GetMethodInformation(uint64_t dex_offset, std::string* method_name,
                                   uint64_t* method_offset) {
  art_api::dex::MethodInfo method_info = GetMethodInfoForOffset(dex_offset, false);
  if (method_info.offset == 0) {
    return false;
  }
  *method_name = method_info.name;
  *method_offset = dex_offset - method_info.offset;
  return true;
}

std::unique_ptr<DexFileFromFile> DexFileFromFile::Create(uint64_t dex_file_offset_in_file,
                                                         const std::string& file) {
  android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(file.c_str(), O_RDONLY | O_CLOEXEC)));
  if (fd == -1) {
    return nullptr;
  }

  std::string error_msg;
  std::unique_ptr<art_api::dex::DexFile> art_dex_file =
      OpenFromFd(fd, dex_file_offset_in_file, file, &error_msg);
  if (art_dex_file == nullptr) {
    return nullptr;
  }

  return std::unique_ptr<DexFileFromFile>(new DexFileFromFile(std::move(*art_dex_file.release())));
}

std::unique_ptr<DexFileFromMemory> DexFileFromMemory::Create(uint64_t dex_file_offset_in_memory,
                                                             Memory* memory,
                                                             const std::string& name) {
  std::vector<uint8_t> backing_memory;

  for (size_t size = 0;;) {
    std::string error_msg;
    std::unique_ptr<art_api::dex::DexFile> art_dex_file =
        OpenFromMemory(backing_memory.data(), &size, name, &error_msg);

    if (art_dex_file != nullptr) {
      return std::unique_ptr<DexFileFromMemory>(
          new DexFileFromMemory(std::move(*art_dex_file.release()), std::move(backing_memory)));
    }

    if (!error_msg.empty()) {
      return nullptr;
    }

    backing_memory.resize(size);
    if (!memory->ReadFully(dex_file_offset_in_memory, backing_memory.data(),
                           backing_memory.size())) {
      return nullptr;
    }
  }
}

}  // namespace unwindstack
