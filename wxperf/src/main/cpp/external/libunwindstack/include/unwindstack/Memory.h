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

#ifndef _LIBUNWINDSTACK_MEMORY_H
#define _LIBUNWINDSTACK_MEMORY_H

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace unwindstack {

class Memory {
 public:
  Memory() = default;
  virtual ~Memory() = default;

  static std::shared_ptr<Memory> CreateProcessMemory(pid_t pid);
  static std::shared_ptr<Memory> CreateProcessMemoryCached(pid_t pid);

  virtual bool ReadString(uint64_t addr, std::string* string, uint64_t max_read = UINT64_MAX);

  virtual void Clear() {}

  virtual size_t Read(uint64_t addr, void* dst, size_t size) = 0;

  bool ReadFully(uint64_t addr, void* dst, size_t size);

  inline bool Read32(uint64_t addr, uint32_t* dst) {
    return ReadFully(addr, dst, sizeof(uint32_t));
  }

  inline bool Read64(uint64_t addr, uint64_t* dst) {
    return ReadFully(addr, dst, sizeof(uint64_t));
  }
};

class MemoryCache : public Memory {
 public:
  MemoryCache(Memory* memory) : impl_(memory) {}
  virtual ~MemoryCache() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  void Clear() override { cache_.clear(); }

 private:
  constexpr static size_t kCacheBits = 12;
  constexpr static size_t kCacheMask = (1 << kCacheBits) - 1;
  constexpr static size_t kCacheSize = 1 << kCacheBits;
  std::unordered_map<uint64_t, uint8_t[kCacheSize]> cache_;

  std::unique_ptr<Memory> impl_;
};

class MemoryBuffer : public Memory {
 public:
  MemoryBuffer() = default;
  virtual ~MemoryBuffer() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  uint8_t* GetPtr(size_t offset);

  void Resize(size_t size) { raw_.resize(size); }

  uint64_t Size() { return raw_.size(); }

 private:
  std::vector<uint8_t> raw_;
};

class MemoryFileAtOffset : public Memory {
 public:
  MemoryFileAtOffset() = default;
  virtual ~MemoryFileAtOffset();

  bool Init(const std::string& file, uint64_t offset, uint64_t size = UINT64_MAX);

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  size_t Size() { return size_; }

  void Clear() override;

 protected:
  size_t size_ = 0;
  size_t offset_ = 0;
  uint8_t* data_ = nullptr;
};

class MemoryRemote : public Memory {
 public:
  MemoryRemote(pid_t pid) : pid_(pid), read_redirect_func_(0) {}
  virtual ~MemoryRemote() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  pid_t pid() { return pid_; }

 private:
  pid_t pid_;
  std::atomic_uintptr_t read_redirect_func_;
};

class MemoryLocal : public Memory {
 public:
  MemoryLocal() = default;
  virtual ~MemoryLocal() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;
};

// MemoryRange maps one address range onto another.
// The range [src_begin, src_begin + length) in the underlying Memory is mapped onto offset,
// such that range.read(offset) is equivalent to underlying.read(src_begin).
class MemoryRange : public Memory {
 public:
  MemoryRange(const std::shared_ptr<Memory>& memory, uint64_t begin, uint64_t length,
              uint64_t offset);
  virtual ~MemoryRange() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  uint64_t offset() { return offset_; }
  uint64_t length() { return length_; }

 private:
  std::shared_ptr<Memory> memory_;
  uint64_t begin_;
  uint64_t length_;
  uint64_t offset_;
};

class MemoryRanges : public Memory {
 public:
  MemoryRanges() = default;
  virtual ~MemoryRanges() = default;

  void Insert(MemoryRange* memory);

  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  std::map<uint64_t, std::unique_ptr<MemoryRange>> maps_;
};

class MemoryOffline : public Memory {
 public:
  MemoryOffline() = default;
  virtual ~MemoryOffline() = default;

  bool Init(const std::string& file, uint64_t offset);

  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  std::unique_ptr<MemoryRange> memory_;
};

class MemoryOfflineBuffer : public Memory {
 public:
  MemoryOfflineBuffer(const uint8_t* data, uint64_t start, uint64_t end);
  virtual ~MemoryOfflineBuffer() = default;

  void Reset(const uint8_t* data, uint64_t start, uint64_t end);

  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  const uint8_t* data_;
  uint64_t start_;
  uint64_t end_;
};

class MemoryOfflineParts : public Memory {
 public:
  MemoryOfflineParts() = default;
  virtual ~MemoryOfflineParts();

  void Add(MemoryOffline* memory) { memories_.push_back(memory); }

  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  std::vector<MemoryOffline*> memories_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_MEMORY_H
