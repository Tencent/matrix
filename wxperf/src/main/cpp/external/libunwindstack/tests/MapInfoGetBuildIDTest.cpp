/*
 * Copyright (C) 2019 The Android Open Source Project
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
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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

class MapInfoGetBuildIDTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tf_.reset(new TemporaryFile);

    memory_ = new MemoryFake;
    elf_ = new ElfFake(new MemoryFake);
    elf_interface_ = new ElfInterfaceFake(memory_);
    elf_->FakeSetInterface(elf_interface_);
    elf_container_.reset(elf_);
    map_info_.reset(new MapInfo(nullptr, 0x1000, 0x20000, 0, PROT_READ | PROT_WRITE, tf_->path));
  }

  void MultipleThreadTest(std::string expected_build_id);

  MemoryFake* memory_;
  ElfFake* elf_;
  ElfInterfaceFake* elf_interface_;
  std::unique_ptr<ElfFake> elf_container_;
  std::unique_ptr<MapInfo> map_info_;
  std::unique_ptr<TemporaryFile> tf_;
};

TEST_F(MapInfoGetBuildIDTest, no_elf_and_no_valid_elf_in_memory) {
  MapInfo info(nullptr, 0x1000, 0x2000, 0, PROT_READ, "");

  EXPECT_EQ("", info.GetBuildID());
  EXPECT_EQ("", info.GetPrintableBuildID());
}

TEST_F(MapInfoGetBuildIDTest, from_elf) {
  map_info_->elf.reset(elf_container_.release());
  elf_interface_->FakeSetBuildID("FAKE_BUILD_ID");

  EXPECT_EQ("FAKE_BUILD_ID", map_info_->GetBuildID());
  EXPECT_EQ("46414b455f4255494c445f4944", map_info_->GetPrintableBuildID());
}

TEST_F(MapInfoGetBuildIDTest, from_elf_no_sign_extension) {
  map_info_->elf.reset(elf_container_.release());

  std::string build_id = {static_cast<char>(0xfa), static_cast<char>(0xab), static_cast<char>(0x12),
                          static_cast<char>(0x02)};
  elf_interface_->FakeSetBuildID(build_id);

  EXPECT_EQ("\xFA\xAB\x12\x2", map_info_->GetBuildID());
  EXPECT_EQ("faab1202", map_info_->GetPrintableBuildID());
}

void MapInfoGetBuildIDTest::MultipleThreadTest(std::string expected_build_id) {
  static constexpr size_t kNumConcurrentThreads = 100;

  std::string build_id_values[kNumConcurrentThreads];
  std::vector<std::thread*> threads;

  std::atomic_bool wait;
  wait = true;
  // Create all of the threads and have them do the GetLoadBias at the same time
  // to make it likely that a race will occur.
  for (size_t i = 0; i < kNumConcurrentThreads; i++) {
    std::thread* thread = new std::thread([i, this, &wait, &build_id_values]() {
      while (wait)
        ;
      build_id_values[i] = map_info_->GetBuildID();
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
    EXPECT_EQ(expected_build_id, build_id_values[i]) << "Thread " << i << " mismatched.";
  }
}

TEST_F(MapInfoGetBuildIDTest, multiple_thread_elf_exists) {
  map_info_->elf.reset(elf_container_.release());
  elf_interface_->FakeSetBuildID("FAKE_BUILD_ID");

  MultipleThreadTest("FAKE_BUILD_ID");
}

static void InitElfData(int fd) {
  Elf32_Ehdr ehdr;
  TestInitEhdr(&ehdr, ELFCLASS32, EM_ARM);
  ehdr.e_shoff = 0x2000;
  ehdr.e_shnum = 3;
  ehdr.e_shentsize = sizeof(Elf32_Shdr);
  ehdr.e_shstrndx = 2;
  off_t offset = 0;
  ASSERT_EQ(offset, lseek(fd, offset, SEEK_SET));
  ASSERT_EQ(static_cast<ssize_t>(sizeof(ehdr)), write(fd, &ehdr, sizeof(ehdr)));

  char note_section[128];
  Elf32_Nhdr note_header = {};
  note_header.n_namesz = 4;   // "GNU"
  note_header.n_descsz = 12;  // "ELF_BUILDID"
  note_header.n_type = NT_GNU_BUILD_ID;
  memcpy(&note_section, &note_header, sizeof(note_header));
  size_t note_offset = sizeof(note_header);
  memcpy(&note_section[note_offset], "GNU", sizeof("GNU"));
  note_offset += sizeof("GNU");
  memcpy(&note_section[note_offset], "ELF_BUILDID", sizeof("ELF_BUILDID"));
  note_offset += sizeof("ELF_BUILDID");

  Elf32_Shdr shdr = {};
  shdr.sh_type = SHT_NOTE;
  shdr.sh_name = 0x500;
  shdr.sh_offset = 0xb000;
  shdr.sh_size = sizeof(note_section);
  offset += ehdr.e_shoff + sizeof(shdr);
  ASSERT_EQ(offset, lseek(fd, offset, SEEK_SET));
  ASSERT_EQ(static_cast<ssize_t>(sizeof(shdr)), write(fd, &shdr, sizeof(shdr)));

  // The string data for section header names.
  memset(&shdr, 0, sizeof(shdr));
  shdr.sh_type = SHT_STRTAB;
  shdr.sh_name = 0x20000;
  shdr.sh_offset = 0xf000;
  shdr.sh_size = 0x1000;
  offset += sizeof(shdr);
  ASSERT_EQ(offset, lseek(fd, offset, SEEK_SET));
  ASSERT_EQ(static_cast<ssize_t>(sizeof(shdr)), write(fd, &shdr, sizeof(shdr)));

  offset = 0xf500;
  ASSERT_EQ(offset, lseek(fd, offset, SEEK_SET));
  ASSERT_EQ(static_cast<ssize_t>(sizeof(".note.gnu.build-id")),
            write(fd, ".note.gnu.build-id", sizeof(".note.gnu.build-id")));

  offset = 0xb000;
  ASSERT_EQ(offset, lseek(fd, offset, SEEK_SET));
  ASSERT_EQ(static_cast<ssize_t>(sizeof(note_section)),
            write(fd, note_section, sizeof(note_section)));
}

TEST_F(MapInfoGetBuildIDTest, from_memory) {
  InitElfData(tf_->fd);

  EXPECT_EQ("ELF_BUILDID", map_info_->GetBuildID());
  EXPECT_EQ("454c465f4255494c444944", map_info_->GetPrintableBuildID());
}

TEST_F(MapInfoGetBuildIDTest, multiple_thread_elf_exists_in_memory) {
  InitElfData(tf_->fd);

  MultipleThreadTest("ELF_BUILDID");
}

}  // namespace unwindstack
