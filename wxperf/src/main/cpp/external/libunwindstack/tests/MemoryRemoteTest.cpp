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
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include <android-base/test_utils.h>
#include <android-base/file.h>
#include <gtest/gtest.h>

#include <unwindstack/Memory.h>

#include "MemoryFake.h"
#include "TestUtils.h"

namespace unwindstack {

class MemoryRemoteTest : public ::testing::Test {
 protected:
  static bool Attach(pid_t pid) {
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
      return false;
    }

    return TestQuiescePid(pid);
  }

  static bool Detach(pid_t pid) {
    return ptrace(PTRACE_DETACH, pid, 0, 0) == 0;
  }

  static constexpr size_t NS_PER_SEC = 1000000000ULL;
};

TEST_F(MemoryRemoteTest, read) {
  std::vector<uint8_t> src(1024);
  memset(src.data(), 0x4c, 1024);

  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true);
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  ASSERT_TRUE(Attach(pid));

  MemoryRemote remote(pid);

  std::vector<uint8_t> dst(1024);
  ASSERT_TRUE(remote.ReadFully(reinterpret_cast<uint64_t>(src.data()), dst.data(), 1024));
  for (size_t i = 0; i < 1024; i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }

  ASSERT_TRUE(Detach(pid));
}

TEST_F(MemoryRemoteTest, read_large) {
  static constexpr size_t kTotalPages = 245;
  std::vector<uint8_t> src(kTotalPages * getpagesize());
  for (size_t i = 0; i < kTotalPages; i++) {
    memset(&src[i * getpagesize()], i, getpagesize());
  }

  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true)
      ;
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  ASSERT_TRUE(Attach(pid));

  MemoryRemote remote(pid);

  std::vector<uint8_t> dst(kTotalPages * getpagesize());
  ASSERT_TRUE(remote.ReadFully(reinterpret_cast<uint64_t>(src.data()), dst.data(), src.size()));
  for (size_t i = 0; i < kTotalPages * getpagesize(); i++) {
    ASSERT_EQ(i / getpagesize(), dst[i]) << "Failed at byte " << i;
  }

  ASSERT_TRUE(Detach(pid));
}

TEST_F(MemoryRemoteTest, read_partial) {
  char* mapping = static_cast<char*>(
      mmap(nullptr, 4 * getpagesize(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  ASSERT_NE(MAP_FAILED, mapping);
  memset(mapping, 0x4c, 4 * getpagesize());
  ASSERT_EQ(0, mprotect(mapping + getpagesize(), getpagesize(), PROT_NONE));
  ASSERT_EQ(0, munmap(mapping + 3 * getpagesize(), getpagesize()));

  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true)
      ;
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  // Unmap from our process.
  ASSERT_EQ(0, munmap(mapping, 3 * getpagesize()));

  ASSERT_TRUE(Attach(pid));

  MemoryRemote remote(pid);

  std::vector<uint8_t> dst(4096);
  size_t bytes =
      remote.Read(reinterpret_cast<uint64_t>(mapping + getpagesize() - 1024), dst.data(), 4096);
  // Some read methods can read PROT_NONE maps, allow that.
  ASSERT_LE(1024U, bytes);
  for (size_t i = 0; i < bytes; i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }

  // Now verify that reading stops at the end of a map.
  bytes =
      remote.Read(reinterpret_cast<uint64_t>(mapping + 3 * getpagesize() - 1024), dst.data(), 4096);
  ASSERT_EQ(1024U, bytes);
  for (size_t i = 0; i < bytes; i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }

  ASSERT_TRUE(Detach(pid));
}

TEST_F(MemoryRemoteTest, read_fail) {
  int pagesize = getpagesize();
  void* src = mmap(nullptr, pagesize * 2, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,-1, 0);
  memset(src, 0x4c, pagesize * 2);
  ASSERT_NE(MAP_FAILED, src);
  // Put a hole right after the first page.
  ASSERT_EQ(0, munmap(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(src) + pagesize),
                      pagesize));

  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true);
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  ASSERT_TRUE(Attach(pid));

  MemoryRemote remote(pid);

  std::vector<uint8_t> dst(pagesize);
  ASSERT_TRUE(remote.ReadFully(reinterpret_cast<uint64_t>(src), dst.data(), pagesize));
  for (size_t i = 0; i < 1024; i++) {
    ASSERT_EQ(0x4cU, dst[i]) << "Failed at byte " << i;
  }

  ASSERT_FALSE(remote.ReadFully(reinterpret_cast<uint64_t>(src) + pagesize, dst.data(), 1));
  ASSERT_TRUE(remote.ReadFully(reinterpret_cast<uint64_t>(src) + pagesize - 1, dst.data(), 1));
  ASSERT_FALSE(remote.ReadFully(reinterpret_cast<uint64_t>(src) + pagesize - 4, dst.data(), 8));

  // Check overflow condition is caught properly.
  ASSERT_FALSE(remote.ReadFully(UINT64_MAX - 100, dst.data(), 200));

  ASSERT_EQ(0, munmap(src, pagesize));

  ASSERT_TRUE(Detach(pid));
}

TEST_F(MemoryRemoteTest, read_overflow) {
  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true)
      ;
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  ASSERT_TRUE(Attach(pid));

  MemoryRemote remote(pid);

  // Check overflow condition is caught properly.
  std::vector<uint8_t> dst(200);
  ASSERT_FALSE(remote.ReadFully(UINT64_MAX - 100, dst.data(), 200));

  ASSERT_TRUE(Detach(pid));
}

TEST_F(MemoryRemoteTest, read_illegal) {
  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true);
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  ASSERT_TRUE(Attach(pid));

  MemoryRemote remote(pid);

  std::vector<uint8_t> dst(100);
  ASSERT_FALSE(remote.ReadFully(0, dst.data(), 1));
  ASSERT_FALSE(remote.ReadFully(0, dst.data(), 100));

  ASSERT_TRUE(Detach(pid));
}

TEST_F(MemoryRemoteTest, read_mprotect_hole) {
  size_t page_size = getpagesize();
  void* mapping =
      mmap(nullptr, 3 * getpagesize(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  ASSERT_NE(MAP_FAILED, mapping);
  memset(mapping, 0xFF, 3 * page_size);
  ASSERT_EQ(0, mprotect(static_cast<char*>(mapping) + page_size, page_size, PROT_NONE));

  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true);
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  ASSERT_EQ(0, munmap(mapping, 3 * page_size));

  ASSERT_TRUE(Attach(pid));

  MemoryRemote remote(pid);
  std::vector<uint8_t> dst(getpagesize() * 4, 0xCC);
  size_t read_size = remote.Read(reinterpret_cast<uint64_t>(mapping), dst.data(), page_size * 3);
  // Some read methods can read PROT_NONE maps, allow that.
  ASSERT_LE(page_size, read_size);
  for (size_t i = 0; i < read_size; ++i) {
    ASSERT_EQ(0xFF, dst[i]);
  }
  for (size_t i = read_size; i < dst.size(); ++i) {
    ASSERT_EQ(0xCC, dst[i]);
  }
}

TEST_F(MemoryRemoteTest, read_munmap_hole) {
  size_t page_size = getpagesize();
  void* mapping =
      mmap(nullptr, 3 * getpagesize(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  ASSERT_NE(MAP_FAILED, mapping);
  memset(mapping, 0xFF, 3 * page_size);
  ASSERT_EQ(0, munmap(static_cast<char*>(mapping) + page_size, page_size));

  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true)
      ;
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  ASSERT_EQ(0, munmap(mapping, page_size));
  ASSERT_EQ(0, munmap(static_cast<char*>(mapping) + 2 * page_size, page_size));

  ASSERT_TRUE(Attach(pid));

  MemoryRemote remote(pid);
  std::vector<uint8_t> dst(getpagesize() * 4, 0xCC);
  size_t read_size = remote.Read(reinterpret_cast<uint64_t>(mapping), dst.data(), page_size * 3);
  ASSERT_EQ(page_size, read_size);
  for (size_t i = 0; i < read_size; ++i) {
    ASSERT_EQ(0xFF, dst[i]);
  }
  for (size_t i = read_size; i < dst.size(); ++i) {
    ASSERT_EQ(0xCC, dst[i]);
  }
}

// Verify that the memory remote object chooses a memory read function
// properly. Either process_vm_readv or ptrace.
TEST_F(MemoryRemoteTest, read_choose_correctly) {
  size_t page_size = getpagesize();
  void* mapping =
      mmap(nullptr, 2 * getpagesize(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  ASSERT_NE(MAP_FAILED, mapping);
  memset(mapping, 0xFC, 2 * page_size);
  ASSERT_EQ(0, mprotect(static_cast<char*>(mapping), page_size, PROT_NONE));

  pid_t pid;
  if ((pid = fork()) == 0) {
    while (true)
      ;
    exit(1);
  }
  ASSERT_LT(0, pid);
  TestScopedPidReaper reap(pid);

  ASSERT_EQ(0, munmap(mapping, 2 * page_size));

  ASSERT_TRUE(Attach(pid));

  // We know that process_vm_readv of a mprotect'd PROT_NONE region will fail.
  // Read from the PROT_NONE area first to force the choice of ptrace.
  MemoryRemote remote_ptrace(pid);
  uint32_t value;
  size_t bytes = remote_ptrace.Read(reinterpret_cast<uint64_t>(mapping), &value, sizeof(value));
  ASSERT_EQ(sizeof(value), bytes);
  ASSERT_EQ(0xfcfcfcfcU, value);
  bytes = remote_ptrace.Read(reinterpret_cast<uint64_t>(mapping) + page_size, &value, sizeof(value));
  ASSERT_EQ(sizeof(value), bytes);
  ASSERT_EQ(0xfcfcfcfcU, value);
  bytes = remote_ptrace.Read(reinterpret_cast<uint64_t>(mapping), &value, sizeof(value));
  ASSERT_EQ(sizeof(value), bytes);
  ASSERT_EQ(0xfcfcfcfcU, value);

  // Now verify that choosing process_vm_readv results in failing reads of
  // the PROT_NONE part of the map. Read from a valid map first which
  // should prefer process_vm_readv, and keep that as the read function.
  MemoryRemote remote_readv(pid);
  bytes = remote_readv.Read(reinterpret_cast<uint64_t>(mapping) + page_size, &value, sizeof(value));
  ASSERT_EQ(sizeof(value), bytes);
  ASSERT_EQ(0xfcfcfcfcU, value);
  bytes = remote_readv.Read(reinterpret_cast<uint64_t>(mapping), &value, sizeof(value));
  ASSERT_EQ(0U, bytes);
  bytes = remote_readv.Read(reinterpret_cast<uint64_t>(mapping) + page_size, &value, sizeof(value));
  ASSERT_EQ(sizeof(value), bytes);
  ASSERT_EQ(0xfcfcfcfcU, value);
}

}  // namespace unwindstack
