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

#pragma once

#include "android-base/macros.h"
#include "android-base/off64_t.h"

#include <sys/types.h>

#include <memory>

#if defined(_WIN32)
#include <windows.h>
#define PROT_READ 1
#define PROT_WRITE 2
#else
#include <sys/mman.h>
#endif

namespace android {
namespace base {

/**
 * A region of a file mapped into memory.
 */
class MappedFile {
 public:
  /**
   * Creates a new mapping of the file pointed to by `fd`. Unlike the underlying OS primitives,
   * `offset` does not need to be page-aligned. If `PROT_WRITE` is set in `prot`, the mapping
   * will be writable, otherwise it will be read-only. Mappings are always `MAP_SHARED`.
   */
  static std::unique_ptr<MappedFile> FromFd(int fd, off64_t offset, size_t length, int prot);

  /**
   * Removes the mapping.
   */
  ~MappedFile();

  char* data() { return base_ + offset_; }
  size_t size() { return size_; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MappedFile);

  char* base_;
  size_t size_;

  size_t offset_;

#if defined(_WIN32)
  MappedFile(char* base, size_t size, size_t offset, HANDLE handle)
      : base_(base), size_(size), offset_(offset), handle_(handle) {}
  HANDLE handle_;
#else
  MappedFile(char* base, size_t size, size_t offset) : base_(base), size_(size), offset_(offset) {}
#endif
};

}  // namespace base
}  // namespace android
