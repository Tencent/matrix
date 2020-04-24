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

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <mutex>
#include <string>

#include <android-base/stringprintf.h>

#include <unwindstack/Elf.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

bool MapInfo::InitFileMemoryFromPreviousReadOnlyMap(MemoryFileAtOffset* memory) {
  // One last attempt, see if the previous map is read-only with the
  // same name and stretches across this map.
  if (prev_map == nullptr || prev_map->flags != PROT_READ) {
    return false;
  }

  uint64_t map_size = end - prev_map->end;
  if (!memory->Init(name, prev_map->offset, map_size)) {
    return false;
  }

  uint64_t max_size;
  if (!Elf::GetInfo(memory, &max_size) || max_size < map_size) {
    return false;
  }

  if (!memory->Init(name, prev_map->offset, max_size)) {
    return false;
  }

  elf_offset = offset - prev_map->offset;
  elf_start_offset = prev_map->offset;
  return true;
}

Memory* MapInfo::GetFileMemory() {
  std::unique_ptr<MemoryFileAtOffset> memory(new MemoryFileAtOffset);
  if (offset == 0) {
    if (memory->Init(name, 0)) {
      return memory.release();
    }
    return nullptr;
  }

  // These are the possibilities when the offset is non-zero.
  // - There is an elf file embedded in a file, and the offset is the
  //   the start of the elf in the file.
  // - There is an elf file embedded in a file, and the offset is the
  //   the start of the executable part of the file. The actual start
  //   of the elf is in the read-only segment preceeding this map.
  // - The whole file is an elf file, and the offset needs to be saved.
  //
  // Map in just the part of the file for the map. If this is not
  // a valid elf, then reinit as if the whole file is an elf file.
  // If the offset is a valid elf, then determine the size of the map
  // and reinit to that size. This is needed because the dynamic linker
  // only maps in a portion of the original elf, and never the symbol
  // file data.
  uint64_t map_size = end - start;
  if (!memory->Init(name, offset, map_size)) {
    return nullptr;
  }

  // Check if the start of this map is an embedded elf.
  uint64_t max_size = 0;
  if (Elf::GetInfo(memory.get(), &max_size)) {
    elf_start_offset = offset;
    if (max_size > map_size) {
      if (memory->Init(name, offset, max_size)) {
        return memory.release();
      }
      // Try to reinit using the default map_size.
      if (memory->Init(name, offset, map_size)) {
        return memory.release();
      }
      elf_start_offset = 0;
      return nullptr;
    }
    return memory.release();
  }

  // No elf at offset, try to init as if the whole file is an elf.
  if (memory->Init(name, 0) && Elf::IsValidElf(memory.get())) {
    elf_offset = offset;
    // Need to check how to set the elf start offset. If this map is not
    // the r-x map of a r-- map, then use the real offset value. Otherwise,
    // use 0.
    if (prev_map == nullptr || prev_map->offset != 0 || prev_map->flags != PROT_READ ||
        prev_map->name != name) {
      elf_start_offset = offset;
    }
    return memory.release();
  }

  // See if the map previous to this one contains a read-only map
  // that represents the real start of the elf data.
  if (InitFileMemoryFromPreviousReadOnlyMap(memory.get())) {
    return memory.release();
  }

  // Failed to find elf at start of file or at read-only map, return
  // file object from the current map.
  if (memory->Init(name, offset, map_size)) {
    return memory.release();
  }
  return nullptr;
}

Memory* MapInfo::CreateMemory(const std::shared_ptr<Memory>& process_memory) {
  if (end <= start) {
    return nullptr;
  }

  elf_offset = 0;

  // Fail on device maps.
  if (flags & MAPS_FLAGS_DEVICE_MAP) {
    return nullptr;
  }

  // First try and use the file associated with the info.
  if (!name.empty()) {
    Memory* memory = GetFileMemory();
    if (memory != nullptr) {
      return memory;
    }
  }

  if (process_memory == nullptr) {
    return nullptr;
  }

  // Need to verify that this elf is valid. It's possible that
  // only part of the elf file to be mapped into memory is in the executable
  // map. In this case, there will be another read-only map that includes the
  // first part of the elf file. This is done if the linker rosegment
  // option is used.
  std::unique_ptr<MemoryRange> memory(new MemoryRange(process_memory, start, end - start, 0));
  if (Elf::IsValidElf(memory.get())) {
    memory_backed_elf = true;
    return memory.release();
  }

  // Find the read-only map by looking at the previous map. The linker
  // doesn't guarantee that this invariant will always be true. However,
  // if that changes, there is likely something else that will change and
  // break something.
  if (offset == 0 || name.empty() || prev_map == nullptr || prev_map->name != name ||
      prev_map->offset >= offset) {
    return nullptr;
  }

  // Make sure that relative pc values are corrected properly.
  elf_offset = offset - prev_map->offset;
  // Use this as the elf start offset, otherwise, you always get offsets into
  // the r-x section, which is not quite the right information.
  elf_start_offset = prev_map->offset;

  MemoryRanges* ranges = new MemoryRanges;
  ranges->Insert(
      new MemoryRange(process_memory, prev_map->start, prev_map->end - prev_map->start, 0));
  ranges->Insert(new MemoryRange(process_memory, start, end - start, elf_offset));

  memory_backed_elf = true;
  return ranges;
}

Elf* MapInfo::GetElf(const std::shared_ptr<Memory>& process_memory, ArchEnum expected_arch) {
  {
    // Make sure no other thread is trying to add the elf to this map.
    std::lock_guard<std::mutex> guard(mutex_);

    if (elf.get() != nullptr) {
      return elf.get();
    }

    bool locked = false;
    if (Elf::CachingEnabled() && !name.empty()) {
      Elf::CacheLock();
      locked = true;
      if (Elf::CacheGet(this)) {
        Elf::CacheUnlock();
        return elf.get();
      }
    }

    Memory* memory = CreateMemory(process_memory);
    if (locked) {
      if (Elf::CacheAfterCreateMemory(this)) {
        delete memory;
        Elf::CacheUnlock();
        return elf.get();
      }
    }
    elf.reset(new Elf(memory));
    // If the init fails, keep the elf around as an invalid object so we
    // don't try to reinit the object.
    elf->Init();
    if (elf->valid() && expected_arch != elf->arch()) {
      // Make the elf invalid, mismatch between arch and expected arch.
      elf->Invalidate();
    }

    if (locked) {
      Elf::CacheAdd(this);
      Elf::CacheUnlock();
    }
  }

  // If there is a read-only map then a read-execute map that represents the
  // same elf object, make sure the previous map is using the same elf
  // object if it hasn't already been set.
  if (prev_map != nullptr && elf_start_offset != offset && prev_map->offset == elf_start_offset &&
      prev_map->name == name) {
    std::lock_guard<std::mutex> guard(prev_map->mutex_);
    if (prev_map->elf.get() == nullptr) {
      prev_map->elf = elf;
      prev_map->memory_backed_elf = memory_backed_elf;
    }
  }
  return elf.get();
}

bool MapInfo::GetFunctionName(uint64_t addr, std::string* name, uint64_t* func_offset) {
  {
    // Make sure no other thread is trying to update this elf object.
    std::lock_guard<std::mutex> guard(mutex_);
    if (elf == nullptr) {
      return false;
    }
  }
  // No longer need the lock, once the elf object is created, it is not deleted
  // until this object is deleted.
  return elf->GetFunctionName(addr, name, func_offset);
}

uint64_t MapInfo::GetLoadBias(const std::shared_ptr<Memory>& process_memory) {
  uint64_t cur_load_bias = load_bias.load();
  if (cur_load_bias != static_cast<uint64_t>(-1)) {
    return cur_load_bias;
  }

  {
    // Make sure no other thread is trying to add the elf to this map.
    std::lock_guard<std::mutex> guard(mutex_);
    if (elf != nullptr) {
      if (elf->valid()) {
        cur_load_bias = elf->GetLoadBias();
        load_bias = cur_load_bias;
        return cur_load_bias;
      } else {
        load_bias = 0;
        return 0;
      }
    }
  }

  // Call lightweight static function that will only read enough of the
  // elf data to get the load bias.
  std::unique_ptr<Memory> memory(CreateMemory(process_memory));
  cur_load_bias = Elf::GetLoadBias(memory.get());
  load_bias = cur_load_bias;
  return cur_load_bias;
}

MapInfo::~MapInfo() {
  uintptr_t id = build_id.load();
  if (id != 0) {
    delete reinterpret_cast<std::string*>(id);
  }
}

std::string MapInfo::GetBuildID() {
  uintptr_t id = build_id.load();
  if (build_id != 0) {
    return *reinterpret_cast<std::string*>(id);
  }

  // No need to lock, at worst if multiple threads do this at the same
  // time it should be detected and only one thread should win and
  // save the data.
  std::unique_ptr<std::string> cur_build_id(new std::string);

  // Now need to see if the elf object exists.
  // Make sure no other thread is trying to add the elf to this map.
  mutex_.lock();
  Elf* elf_obj = elf.get();
  mutex_.unlock();
  if (elf_obj != nullptr) {
    *cur_build_id = elf_obj->GetBuildID();
  } else {
    // This will only work if we can get the file associated with this memory.
    // If this is only available in memory, then the section name information
    // is not present and we will not be able to find the build id info.
    std::unique_ptr<Memory> memory(GetFileMemory());
    if (memory != nullptr) {
      *cur_build_id = Elf::GetBuildID(memory.get());
    }
  }

  id = reinterpret_cast<uintptr_t>(cur_build_id.get());
  uintptr_t expected_id = 0;
  if (build_id.compare_exchange_weak(expected_id, id)) {
    // Value saved, so make sure the memory is not freed.
    cur_build_id.release();
  }
  return *reinterpret_cast<std::string*>(id);
}

std::string MapInfo::GetPrintableBuildID() {
  std::string raw_build_id = GetBuildID();
  if (raw_build_id.empty()) {
    return "";
  }
  std::string printable_build_id;
  for (const char& c : raw_build_id) {
    // Use %hhx to avoid sign extension on abis that have signed chars.
    printable_build_id += android::base::StringPrintf("%02hhx", c);
  }
  return printable_build_id;
}

}  // namespace unwindstack
