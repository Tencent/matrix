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

#include <elf.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <android-base/file.h>
#include <android-base/test_utils.h>
#include <gtest/gtest.h>

#include <unwindstack/Elf.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

#include "ElfFake.h"
#include "ElfTestUtils.h"
#include "MemoryFake.h"

namespace unwindstack {

class MapInfoGetLoadBiasTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_ = new MemoryFake;
    process_memory_.reset(memory_);
    elf_ = new ElfFake(new MemoryFake);
    elf_container_.reset(elf_);
    map_info_.reset(new MapInfo(nullptr, 0x1000, 0x20000, 0, PROT_READ | PROT_WRITE, ""));
  }

  void MultipleThreadTest(uint64_t expected_load_bias);

  std::shared_ptr<Memory> process_memory_;
  MemoryFake* memory_;
  ElfFake* elf_;
  std::unique_ptr<ElfFake> elf_container_;
  std::unique_ptr<MapInfo> map_info_;
};

TEST_F(MapInfoGetLoadBiasTest, no_elf_and_no_valid_elf_in_memory) {
  MapInfo info(nullptr, 0x1000, 0x2000, 0, PROT_READ, "");

  EXPECT_EQ(0U, info.GetLoadBias(process_memory_));
}

TEST_F(MapInfoGetLoadBiasTest, load_bias_cached_from_elf) {
  map_info_->elf.reset(elf_container_.release());

  elf_->FakeSetLoadBias(0);
  EXPECT_EQ(0U, map_info_->GetLoadBias(process_memory_));

  elf_->FakeSetLoadBias(0x1000);
  EXPECT_EQ(0U, map_info_->GetLoadBias(process_memory_));
}

TEST_F(MapInfoGetLoadBiasTest, elf_exists) {
  map_info_->elf.reset(elf_container_.release());

  elf_->FakeSetLoadBias(0);
  EXPECT_EQ(0U, map_info_->GetLoadBias(process_memory_));

  map_info_->load_bias = static_cast<uint64_t>(-1);
  elf_->FakeSetLoadBias(0x1000);
  EXPECT_EQ(0x1000U, map_info_->GetLoadBias(process_memory_));
}

void MapInfoGetLoadBiasTest::MultipleThreadTest(uint64_t expected_load_bias) {
  static constexpr size_t kNumConcurrentThreads = 100;

  uint64_t load_bias_values[kNumConcurrentThreads];
  std::vector<std::thread*> threads;

  std::atomic_bool wait;
  wait = true;
  // Create all of the threads and have them do the GetLoadBias at the same time
  // to make it likely that a race will occur.
  for (size_t i = 0; i < kNumConcurrentThreads; i++) {
    std::thread* thread = new std::thread([i, this, &wait, &load_bias_values]() {
      while (wait)
        ;
      load_bias_values[i] = map_info_->GetLoadBias(process_memory_);
    });
    threads.push_back(thread);
  }

  // Set them all going and wait for the threads to finish.
  wait = false;
  for (auto thread : threads) {
    thread->join();
    delete thread;
  }

  // Now verify that all of the elf files are exactly the same and valid.
  for (size_t i = 0; i < kNumConcurrentThreads; i++) {
    EXPECT_EQ(expected_load_bias, load_bias_values[i]) << "Thread " << i << " mismatched.";
  }
}

TEST_F(MapInfoGetLoadBiasTest, multiple_thread_elf_exists) {
  map_info_->elf.reset(elf_container_.release());
  elf_->FakeSetLoadBias(0x1000);

  MultipleThreadTest(0x1000);
}

static void InitElfData(MemoryFake* memory, uint64_t offset) {
  Elf32_Ehdr ehdr;
  TestInitEhdr(&ehdr, ELFCLASS32, EM_ARM);
  ehdr.e_phoff = 0x5000;
  ehdr.e_phnum = 2;
  ehdr.e_phentsize = sizeof(Elf32_Phdr);
  memory->SetMemory(offset, &ehdr, sizeof(ehdr));

  Elf32_Phdr phdr;
  memset(&phdr, 0, sizeof(phdr));
  phdr.p_type = PT_NULL;
  memory->SetMemory(offset + 0x5000, &phdr, sizeof(phdr));
  phdr.p_type = PT_LOAD;
  phdr.p_offset = 0;
  phdr.p_vaddr = 0xe000;
  memory->SetMemory(offset + 0x5000 + sizeof(phdr), &phdr, sizeof(phdr));
}

TEST_F(MapInfoGetLoadBiasTest, elf_exists_in_memory) {
  InitElfData(memory_, map_info_->start);

  EXPECT_EQ(0xe000U, map_info_->GetLoadBias(process_memory_));
}

TEST_F(MapInfoGetLoadBiasTest, elf_exists_in_memory_cached) {
  InitElfData(memory_, map_info_->start);

  EXPECT_EQ(0xe000U, map_info_->GetLoadBias(process_memory_));

  memory_->Clear();
  EXPECT_EQ(0xe000U, map_info_->GetLoadBias(process_memory_));
}

TEST_F(MapInfoGetLoadBiasTest, multiple_thread_elf_exists_in_memory) {
  InitElfData(memory_, map_info_->start);

  MultipleThreadTest(0xe000);
}

}  // namespace unwindstack
