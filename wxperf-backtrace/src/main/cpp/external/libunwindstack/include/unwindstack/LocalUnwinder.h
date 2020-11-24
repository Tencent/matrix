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

#ifndef _LIBUNWINDSTACK_LOCAL_UNWINDER_H
#define _LIBUNWINDSTACK_LOCAL_UNWINDER_H

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include <unwindstack/Error.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

// Forward declarations.
class Elf;
struct MapInfo;

struct LocalFrameData {
  LocalFrameData(MapInfo* map_info, uint64_t pc, uint64_t rel_pc, const std::string& function_name,
                 uint64_t function_offset)
      : map_info(map_info),
        pc(pc),
        rel_pc(rel_pc),
        function_name(function_name),
        function_offset(function_offset) {}

  MapInfo* map_info;
  uint64_t pc;
  uint64_t rel_pc;
  std::string function_name;
  uint64_t function_offset;
};

// This is a specialized class that should only be used for doing local unwinds.
// The Unwind call can be made as multiple times on the same object, and it can
// be called by multiple threads at the same time.
// It is designed to be used in debugging circumstances to get a stack trace
// as fast as possible.
class LocalUnwinder {
 public:
  LocalUnwinder() = default;
  LocalUnwinder(const std::vector<std::string>& skip_libraries) : skip_libraries_(skip_libraries) {}
  ~LocalUnwinder() = default;

  bool Init();

  bool Unwind(std::vector<LocalFrameData>* frame_info, size_t max_frames);

  bool ShouldSkipLibrary(const std::string& map_name);

  MapInfo* GetMapInfo(uint64_t pc);

  ErrorCode LastErrorCode() { return last_error_.code; }
  uint64_t LastErrorAddress() { return last_error_.address; }

 private:
  pthread_rwlock_t maps_rwlock_;
  std::unique_ptr<LocalUpdatableMaps> maps_ = nullptr;
  std::shared_ptr<Memory> process_memory_;
  std::vector<std::string> skip_libraries_;
  ErrorData last_error_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_LOCAL_UNWINDER_H
