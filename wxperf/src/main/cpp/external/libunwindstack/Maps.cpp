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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/unique_fd.h>
#include <procinfo/process_map.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include <unwindstack/Elf.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

MapInfo* Maps::Find(uint64_t pc) {
  if (maps_.empty()) {
    return nullptr;
  }
  size_t first = 0;
  size_t last = maps_.size();
  while (first < last) {
    size_t index = (first + last) / 2;
    const auto& cur = maps_[index];
    if (pc >= cur->start && pc < cur->end) {
      return cur.get();
    } else if (pc < cur->start) {
      last = index;
    } else {
      first = index + 1;
    }
  }
  return nullptr;
}

bool Maps::Parse() {
  return android::procinfo::ReadMapFile(
      GetMapsFile(),
      [&](uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff, ino_t, const char* name) {
        // Mark a device map in /dev/ and not in /dev/ashmem/ specially.
        if (strncmp(name, "/dev/", 5) == 0 && strncmp(name + 5, "ashmem/", 7) != 0) {
          flags |= unwindstack::MAPS_FLAGS_DEVICE_MAP;
        }
        maps_.emplace_back(
            new MapInfo(maps_.empty() ? nullptr : maps_.back().get(), start, end, pgoff,
                        flags, name));
      });
}

void Maps::Add(uint64_t start, uint64_t end, uint64_t offset, uint64_t flags,
               const std::string& name, uint64_t load_bias) {
  auto map_info =
      std::make_unique<MapInfo>(maps_.empty() ? nullptr : maps_.back().get(), start, end, offset,
                                flags, name);
  map_info->load_bias = load_bias;
  maps_.emplace_back(std::move(map_info));
}

void Maps::Sort() {
  std::sort(maps_.begin(), maps_.end(),
            [](const std::unique_ptr<MapInfo>& a, const std::unique_ptr<MapInfo>& b) {
              return a->start < b->start; });

  // Set the prev_map values on the info objects.
  MapInfo* prev_map = nullptr;
  for (const auto& map_info : maps_) {
    map_info->prev_map = prev_map;
    prev_map = map_info.get();
  }
}

bool BufferMaps::Parse() {
  std::string content(buffer_);
  return android::procinfo::ReadMapFileContent(
      &content[0],
      [&](uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff, ino_t, const char* name) {
        // Mark a device map in /dev/ and not in /dev/ashmem/ specially.
        if (strncmp(name, "/dev/", 5) == 0 && strncmp(name + 5, "ashmem/", 7) != 0) {
          flags |= unwindstack::MAPS_FLAGS_DEVICE_MAP;
        }
        maps_.emplace_back(
            new MapInfo(maps_.empty() ? nullptr : maps_.back().get(), start, end, pgoff,
                        flags, name));
      });
}

const std::string RemoteMaps::GetMapsFile() const {
  return "/proc/" + std::to_string(pid_) + "/maps";
}

const std::string LocalUpdatableMaps::GetMapsFile() const {
  return "/proc/self/maps";
}

bool LocalUpdatableMaps::Reparse() {
  // New maps will be added at the end without deleting the old ones.
  size_t last_map_idx = maps_.size();
  if (!Parse()) {
    maps_.resize(last_map_idx);
    return false;
  }

  size_t total_entries = maps_.size();
  size_t search_map_idx = 0;
  for (size_t new_map_idx = last_map_idx; new_map_idx < maps_.size(); new_map_idx++) {
    auto& new_map_info = maps_[new_map_idx];
    uint64_t start = new_map_info->start;
    uint64_t end = new_map_info->end;
    uint64_t flags = new_map_info->flags;
    std::string* name = &new_map_info->name;
    for (size_t old_map_idx = search_map_idx; old_map_idx < last_map_idx; old_map_idx++) {
      auto& info = maps_[old_map_idx];
      if (start == info->start && end == info->end && flags == info->flags && *name == info->name) {
        // No need to check
        search_map_idx = old_map_idx + 1;
        maps_[new_map_idx] = nullptr;
        total_entries--;
        break;
      } else if (info->start > start) {
        // Stop, there isn't going to be a match.
        search_map_idx = old_map_idx;
        break;
      }

      // Never delete these maps, they may be in use. The assumption is
      // that there will only every be a handfull of these so waiting
      // to destroy them is not too expensive.
      saved_maps_.emplace_back(std::move(info));
      maps_[old_map_idx] = nullptr;
      total_entries--;
    }
    if (search_map_idx >= last_map_idx) {
      break;
    }
  }

  // Now move out any of the maps that never were found.
  for (size_t i = search_map_idx; i < last_map_idx; i++) {
    saved_maps_.emplace_back(std::move(maps_[i]));
    maps_[i] = nullptr;
    total_entries--;
  }

  // Sort all of the values such that the nullptrs wind up at the end, then
  // resize them away.
  std::sort(maps_.begin(), maps_.end(), [](const auto& a, const auto& b) {
    if (a == nullptr) {
      return false;
    } else if (b == nullptr) {
      return true;
    }
    return a->start < b->start;
  });
  maps_.resize(total_entries);

  return true;
}

}  // namespace unwindstack
