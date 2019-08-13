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

#include <elf.h>
#include <stdint.h>
#include <sys/mman.h>

#include <memory>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include <unwindstack/Elf.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsArm.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/RegsX86.h>
#include <unwindstack/RegsX86_64.h>
#include <unwindstack/RegsMips.h>
#include <unwindstack/RegsMips64.h>
#include <unwindstack/Unwinder.h>

#include "ElfFake.h"
#include "MemoryFake.h"
#include "RegsFake.h"

namespace unwindstack {

class MapsFake : public Maps {
 public:
  MapsFake() = default;
  virtual ~MapsFake() = default;

  bool Parse() { return true; }

  void FakeClear() { maps_.clear(); }

  void FakeAddMapInfo(MapInfo* map_info) { maps_.push_back(map_info); }
};

class UnwinderTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    maps_.FakeClear();
    MapInfo* info = new MapInfo(0x1000, 0x8000, 0, PROT_READ | PROT_WRITE, "/system/fake/libc.so");
    ElfFake* elf = new ElfFake(new MemoryFake);
    info->elf.reset(elf);
    elf->FakeSetInterface(new ElfInterfaceFake(nullptr));
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0x10000, 0x12000, 0, PROT_READ | PROT_WRITE, "[stack]");
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0x13000, 0x15000, 0, PROT_READ | PROT_WRITE | MAPS_FLAGS_DEVICE_MAP,
                       "/dev/fake_device");
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0x20000, 0x22000, 0, PROT_READ | PROT_WRITE, "/system/fake/libunwind.so");
    elf = new ElfFake(new MemoryFake);
    info->elf.reset(elf);
    elf->FakeSetInterface(new ElfInterfaceFake(nullptr));
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0x23000, 0x24000, 0, PROT_READ | PROT_WRITE, "/fake/libanother.so");
    elf = new ElfFake(new MemoryFake);
    info->elf.reset(elf);
    elf->FakeSetInterface(new ElfInterfaceFake(nullptr));
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0x33000, 0x34000, 0, PROT_READ | PROT_WRITE, "/fake/compressed.so");
    elf = new ElfFake(new MemoryFake);
    info->elf.reset(elf);
    elf->FakeSetInterface(new ElfInterfaceFake(nullptr));
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0x43000, 0x44000, 0x1d000, PROT_READ | PROT_WRITE, "/fake/fake.apk");
    elf = new ElfFake(new MemoryFake);
    info->elf.reset(elf);
    elf->FakeSetInterface(new ElfInterfaceFake(nullptr));
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0x53000, 0x54000, 0, PROT_READ | PROT_WRITE, "/fake/fake.oat");
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0xa3000, 0xa4000, 0, PROT_READ | PROT_WRITE | PROT_EXEC, "/fake/fake.vdex");
    info->load_bias = 0;
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0xa5000, 0xa6000, 0, PROT_READ | PROT_WRITE | PROT_EXEC,
                       "/fake/fake_load_bias.so");
    elf = new ElfFake(new MemoryFake);
    info->elf.reset(elf);
    elf->FakeSetInterface(new ElfInterfaceFake(nullptr));
    elf->FakeSetLoadBias(0x5000);
    maps_.FakeAddMapInfo(info);

    info = new MapInfo(0xa7000, 0xa8000, 0, PROT_READ | PROT_WRITE | PROT_EXEC,
                       "/fake/fake_offset.oat");
    elf = new ElfFake(new MemoryFake);
    info->elf.reset(elf);
    elf->FakeSetInterface(new ElfInterfaceFake(nullptr));
    info->elf_offset = 0x8000;
    maps_.FakeAddMapInfo(info);

    process_memory_.reset(new MemoryFake);
  }

  void SetUp() override {
    ElfInterfaceFake::FakeClear();
    regs_.FakeSetArch(ARCH_ARM);
    regs_.FakeSetReturnAddressValid(false);
  }

  static MapsFake maps_;
  static RegsFake regs_;
  static std::shared_ptr<Memory> process_memory_;
};

MapsFake UnwinderTest::maps_;
RegsFake UnwinderTest::regs_(5);
std::shared_ptr<Memory> UnwinderTest::process_memory_(nullptr);

TEST_F(UnwinderTest, multiple_frames) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame2", 2));

  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0x1102, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x1202, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(3U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0x100U, frame->rel_pc);
  EXPECT_EQ(0x1100U, frame->pc);
  EXPECT_EQ(0x10010U, frame->sp);
  EXPECT_EQ("Frame1", frame->function_name);
  EXPECT_EQ(1U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[2];
  EXPECT_EQ(2U, frame->num);
  EXPECT_EQ(0x200U, frame->rel_pc);
  EXPECT_EQ(0x1200U, frame->pc);
  EXPECT_EQ(0x10020U, frame->sp);
  EXPECT_EQ("Frame2", frame->function_name);
  EXPECT_EQ(2U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

TEST_F(UnwinderTest, multiple_frames_dont_resolve_names) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame2", 2));

  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0x1102, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x1202, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.SetResolveNames(false);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(3U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0x100U, frame->rel_pc);
  EXPECT_EQ(0x1100U, frame->pc);
  EXPECT_EQ(0x10010U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[2];
  EXPECT_EQ(2U, frame->num);
  EXPECT_EQ(0x200U, frame->rel_pc);
  EXPECT_EQ(0x1200U, frame->pc);
  EXPECT_EQ(0x10020U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

TEST_F(UnwinderTest, non_zero_load_bias) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));

  regs_.set_pc(0xa5500);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(1U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0x5500U, frame->rel_pc);
  EXPECT_EQ(0xa5500U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/fake/fake_load_bias.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0xa5000U, frame->map_start);
  EXPECT_EQ(0xa6000U, frame->map_end);
  EXPECT_EQ(0x5000U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, frame->map_flags);
}

TEST_F(UnwinderTest, non_zero_elf_offset) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));

  regs_.set_pc(0xa7500);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(1U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0x8500U, frame->rel_pc);
  EXPECT_EQ(0xa7500U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/fake/fake_offset.oat", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0xa7000U, frame->map_start);
  EXPECT_EQ(0xa8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, frame->map_flags);
}

TEST_F(UnwinderTest, non_zero_map_offset) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));

  regs_.set_pc(0x43000);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(1U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x43000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/fake/fake.apk", frame->map_name);
  EXPECT_EQ(0x1d000U, frame->map_offset);
  EXPECT_EQ(0x43000U, frame->map_start);
  EXPECT_EQ(0x44000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

// Verify that no attempt to continue after the step indicates it is done.
TEST_F(UnwinderTest, no_frames_after_finished) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame2", 2));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame3", 3));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame4", 4));

  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0x1000, 0x10000, true));
  ElfInterfaceFake::FakePushStepData(StepData(0x1102, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x1202, 0x10020, false));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(1U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

// Verify the maximum frames to save.
TEST_F(UnwinderTest, max_frames) {
  for (size_t i = 0; i < 30; i++) {
    ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame" + std::to_string(i), i));
    ElfInterfaceFake::FakePushStepData(StepData(0x1102 + i * 0x100, 0x10010 + i * 0x10, false));
  }

  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);

  Unwinder unwinder(20, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_MAX_FRAMES_EXCEEDED, unwinder.LastErrorCode());

  ASSERT_EQ(20U, unwinder.NumFrames());

  for (size_t i = 0; i < 20; i++) {
    auto* frame = &unwinder.frames()[i];
    EXPECT_EQ(i, frame->num);
    EXPECT_EQ(i * 0x100, frame->rel_pc) << "Failed at frame " << i;
    EXPECT_EQ(0x1000 + i * 0x100, frame->pc) << "Failed at frame " << i;
    EXPECT_EQ(0x10000 + 0x10 * i, frame->sp) << "Failed at frame " << i;
    EXPECT_EQ("Frame" + std::to_string(i), frame->function_name) << "Failed at frame " << i;
    EXPECT_EQ(i, frame->function_offset) << "Failed at frame " << i;
    EXPECT_EQ("/system/fake/libc.so", frame->map_name) << "Failed at frame " << i;
    EXPECT_EQ(0U, frame->map_offset) << "Failed at frame " << i;
    EXPECT_EQ(0x1000U, frame->map_start) << "Failed at frame " << i;
    EXPECT_EQ(0x8000U, frame->map_end) << "Failed at frame " << i;
    EXPECT_EQ(0U, frame->map_load_bias) << "Failed at frame " << i;
    EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags) << "Failed at frame " << i;
  }
}

// Verify that initial map names frames are removed.
TEST_F(UnwinderTest, verify_frames_skipped) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame2", 2));

  regs_.set_pc(0x20000);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0x23002, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x23102, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x20002, 0x10030, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x21002, 0x10040, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x1002, 0x10050, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x21002, 0x10060, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x23002, 0x10070, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  std::vector<std::string> skip_libs{"libunwind.so", "libanother.so"};
  unwinder.Unwind(&skip_libs);
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(3U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10050U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0x1000U, frame->rel_pc);
  EXPECT_EQ(0x21000U, frame->pc);
  EXPECT_EQ(0x10060U, frame->sp);
  EXPECT_EQ("Frame1", frame->function_name);
  EXPECT_EQ(1U, frame->function_offset);
  EXPECT_EQ("/system/fake/libunwind.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x20000U, frame->map_start);
  EXPECT_EQ(0x22000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[2];
  EXPECT_EQ(2U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x23000U, frame->pc);
  EXPECT_EQ(0x10070U, frame->sp);
  EXPECT_EQ("Frame2", frame->function_name);
  EXPECT_EQ(2U, frame->function_offset);
  EXPECT_EQ("/fake/libanother.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x23000U, frame->map_start);
  EXPECT_EQ(0x24000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

// Verify SP in a non-existant map is okay.
TEST_F(UnwinderTest, sp_not_in_map) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));

  regs_.set_pc(0x1000);
  regs_.set_sp(0x63000);
  ElfInterfaceFake::FakePushStepData(StepData(0x21002, 0x50020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(2U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x63000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0x1000U, frame->rel_pc);
  EXPECT_EQ(0x21000U, frame->pc);
  EXPECT_EQ(0x50020U, frame->sp);
  EXPECT_EQ("Frame1", frame->function_name);
  EXPECT_EQ(1U, frame->function_offset);
  EXPECT_EQ("/system/fake/libunwind.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x20000U, frame->map_start);
  EXPECT_EQ(0x22000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

// Verify PC in a device stops the unwind.
TEST_F(UnwinderTest, pc_in_device_stops_unwind) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame2", 2));

  regs_.set_pc(0x13000);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0x23002, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x23102, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(1U, unwinder.NumFrames());
}

// Verify SP in a device stops the unwind.
TEST_F(UnwinderTest, sp_in_device_stops_unwind) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame2", 2));

  regs_.set_pc(0x1000);
  regs_.set_sp(0x13000);
  ElfInterfaceFake::FakePushStepData(StepData(0x23002, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x23102, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(1U, unwinder.NumFrames());
}

// Verify a no map info frame gets a frame.
TEST_F(UnwinderTest, pc_without_map) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));

  regs_.set_pc(0x41000);
  regs_.set_sp(0x13000);

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_INVALID_MAP, unwinder.LastErrorCode());

  ASSERT_EQ(1U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0x41000U, frame->rel_pc);
  EXPECT_EQ(0x41000U, frame->pc);
  EXPECT_EQ(0x13000U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0U, frame->map_start);
  EXPECT_EQ(0U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(0, frame->map_flags);
}

// Verify that a speculative frame is added.
TEST_F(UnwinderTest, speculative_frame) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));

  // Fake as if code called a nullptr function.
  regs_.set_pc(0);
  regs_.set_sp(0x10000);
  regs_.FakeSetReturnAddress(0x1202);
  regs_.FakeSetReturnAddressValid(true);

  ElfInterfaceFake::FakePushStepData(StepData(0x23102, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(3U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0U, frame->map_start);
  EXPECT_EQ(0U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(0, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0x200U, frame->rel_pc);
  EXPECT_EQ(0x1200U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[2];
  EXPECT_EQ(2U, frame->num);
  EXPECT_EQ(0x100U, frame->rel_pc);
  EXPECT_EQ(0x23100U, frame->pc);
  EXPECT_EQ(0x10020U, frame->sp);
  EXPECT_EQ("Frame1", frame->function_name);
  EXPECT_EQ(1U, frame->function_offset);
  EXPECT_EQ("/fake/libanother.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x23000U, frame->map_start);
  EXPECT_EQ(0x24000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

// Verify that a speculative frame is added then removed because no other
// frames are added.
TEST_F(UnwinderTest, speculative_frame_removed) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));

  // Fake as if code called a nullptr function.
  regs_.set_pc(0);
  regs_.set_sp(0x10000);
  regs_.FakeSetReturnAddress(0x1202);
  regs_.FakeSetReturnAddressValid(true);

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(1U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0U, frame->map_start);
  EXPECT_EQ(0U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(0, frame->map_flags);
}

// Verify that an unwind stops when a frame is in given suffix.
TEST_F(UnwinderTest, map_ignore_suffixes) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame2", 2));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame3", 3));

  // Fake as if code called a nullptr function.
  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0x43402, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x53502, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  std::vector<std::string> suffixes{"oat"};
  unwinder.Unwind(nullptr, &suffixes);
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(2U, unwinder.NumFrames());
  // Make sure the elf was not initialized.
  MapInfo* map_info = maps_.Find(0x53000);
  ASSERT_TRUE(map_info != nullptr);
  EXPECT_TRUE(map_info->elf == nullptr);

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0x400U, frame->rel_pc);
  EXPECT_EQ(0x43400U, frame->pc);
  EXPECT_EQ(0x10010U, frame->sp);
  EXPECT_EQ("Frame1", frame->function_name);
  EXPECT_EQ(1U, frame->function_offset);
  EXPECT_EQ("/fake/fake.apk", frame->map_name);
  EXPECT_EQ(0x1d000U, frame->map_offset);
  EXPECT_EQ(0x43000U, frame->map_start);
  EXPECT_EQ(0x44000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

// Verify that an unwind stops when the sp and pc don't change.
TEST_F(UnwinderTest, sp_pc_do_not_change) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame2", 2));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame3", 3));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame4", 4));

  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);
  ElfInterfaceFake::FakePushStepData(StepData(0x33402, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x33502, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x33502, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x33502, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0x33502, 0x10020, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_REPEATED_FRAME, unwinder.LastErrorCode());

  ASSERT_EQ(3U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0x400U, frame->rel_pc);
  EXPECT_EQ(0x33400U, frame->pc);
  EXPECT_EQ(0x10010U, frame->sp);
  EXPECT_EQ("Frame1", frame->function_name);
  EXPECT_EQ(1U, frame->function_offset);
  EXPECT_EQ("/fake/compressed.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x33000U, frame->map_start);
  EXPECT_EQ(0x34000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[2];
  EXPECT_EQ(2U, frame->num);
  EXPECT_EQ(0x500U, frame->rel_pc);
  EXPECT_EQ(0x33500U, frame->pc);
  EXPECT_EQ(0x10020U, frame->sp);
  EXPECT_EQ("Frame2", frame->function_name);
  EXPECT_EQ(2U, frame->function_offset);
  EXPECT_EQ("/fake/compressed.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x33000U, frame->map_start);
  EXPECT_EQ(0x34000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

TEST_F(UnwinderTest, dex_pc_in_map) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);
  regs_.FakeSetDexPc(0xa3400);

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(2U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0x400U, frame->rel_pc);
  EXPECT_EQ(0xa3400U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/fake/fake.vdex", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0xa3000U, frame->map_start);
  EXPECT_EQ(0xa4000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

TEST_F(UnwinderTest, dex_pc_not_in_map) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);
  regs_.FakeSetDexPc(0x50000);

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(2U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0x50000U, frame->rel_pc);
  EXPECT_EQ(0x50000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0U, frame->map_start);
  EXPECT_EQ(0U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(0, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

TEST_F(UnwinderTest, dex_pc_multiple_frames) {
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 0));
  ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame1", 1));
  regs_.set_pc(0x1000);
  regs_.set_sp(0x10000);
  regs_.FakeSetDexPc(0xa3400);
  ElfInterfaceFake::FakePushStepData(StepData(0x33402, 0x10010, false));
  ElfInterfaceFake::FakePushStepData(StepData(0, 0, true));

  Unwinder unwinder(64, &maps_, &regs_, process_memory_);
  unwinder.Unwind();
  EXPECT_EQ(ERROR_NONE, unwinder.LastErrorCode());

  ASSERT_EQ(3U, unwinder.NumFrames());

  auto* frame = &unwinder.frames()[0];
  EXPECT_EQ(0U, frame->num);
  EXPECT_EQ(0x400U, frame->rel_pc);
  EXPECT_EQ(0xa3400U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/fake/fake.vdex", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0xa3000U, frame->map_start);
  EXPECT_EQ(0xa4000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE | PROT_EXEC, frame->map_flags);

  frame = &unwinder.frames()[1];
  EXPECT_EQ(1U, frame->num);
  EXPECT_EQ(0U, frame->rel_pc);
  EXPECT_EQ(0x1000U, frame->pc);
  EXPECT_EQ(0x10000U, frame->sp);
  EXPECT_EQ("Frame0", frame->function_name);
  EXPECT_EQ(0U, frame->function_offset);
  EXPECT_EQ("/system/fake/libc.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x1000U, frame->map_start);
  EXPECT_EQ(0x8000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);

  frame = &unwinder.frames()[2];
  EXPECT_EQ(2U, frame->num);
  EXPECT_EQ(0x400U, frame->rel_pc);
  EXPECT_EQ(0x33400U, frame->pc);
  EXPECT_EQ(0x10010U, frame->sp);
  EXPECT_EQ("Frame1", frame->function_name);
  EXPECT_EQ(1U, frame->function_offset);
  EXPECT_EQ("/fake/compressed.so", frame->map_name);
  EXPECT_EQ(0U, frame->map_offset);
  EXPECT_EQ(0x33000U, frame->map_start);
  EXPECT_EQ(0x34000U, frame->map_end);
  EXPECT_EQ(0U, frame->map_load_bias);
  EXPECT_EQ(PROT_READ | PROT_WRITE, frame->map_flags);
}

// Verify format frame code.
TEST_F(UnwinderTest, format_frame_static) {
  FrameData frame;
  frame.num = 1;
  frame.rel_pc = 0x1000;
  frame.pc = 0x4000;
  frame.sp = 0x1000;
  frame.function_name = "function";
  frame.function_offset = 100;
  frame.map_name = "/fake/libfake.so";
  frame.map_offset = 0x2000;
  frame.map_start = 0x3000;
  frame.map_end = 0x6000;
  frame.map_flags = PROT_READ;

  EXPECT_EQ("  #01 pc 0000000000001000 (offset 0x2000)  /fake/libfake.so (function+100)",
            Unwinder::FormatFrame(frame, false));
  EXPECT_EQ("  #01 pc 00001000 (offset 0x2000)  /fake/libfake.so (function+100)",
            Unwinder::FormatFrame(frame, true));

  frame.map_offset = 0;
  EXPECT_EQ("  #01 pc 0000000000001000  /fake/libfake.so (function+100)",
            Unwinder::FormatFrame(frame, false));
  EXPECT_EQ("  #01 pc 00001000  /fake/libfake.so (function+100)",
            Unwinder::FormatFrame(frame, true));

  frame.function_offset = 0;
  EXPECT_EQ("  #01 pc 0000000000001000  /fake/libfake.so (function)",
            Unwinder::FormatFrame(frame, false));
  EXPECT_EQ("  #01 pc 00001000  /fake/libfake.so (function)", Unwinder::FormatFrame(frame, true));

  frame.function_name = "";
  EXPECT_EQ("  #01 pc 0000000000001000  /fake/libfake.so", Unwinder::FormatFrame(frame, false));
  EXPECT_EQ("  #01 pc 00001000  /fake/libfake.so", Unwinder::FormatFrame(frame, true));

  frame.map_name = "";
  EXPECT_EQ("  #01 pc 0000000000001000  <anonymous:3000>", Unwinder::FormatFrame(frame, false));
  EXPECT_EQ("  #01 pc 00001000  <anonymous:3000>", Unwinder::FormatFrame(frame, true));

  frame.map_start = 0;
  frame.map_end = 0;
  EXPECT_EQ("  #01 pc 0000000000001000  <unknown>", Unwinder::FormatFrame(frame, false));
  EXPECT_EQ("  #01 pc 00001000  <unknown>", Unwinder::FormatFrame(frame, true));
}

static std::string ArchToString(ArchEnum arch) {
  if (arch == ARCH_ARM) {
    return "Arm";
  } else if (arch == ARCH_ARM64) {
    return "Arm64";
  } else if (arch == ARCH_X86) {
    return "X86";
  } else if (arch == ARCH_X86_64) {
    return "X86_64";
  } else {
    return "Unknown";
  }
}

// Verify format frame code.
TEST_F(UnwinderTest, format_frame) {
  std::vector<Regs*> reg_list;
  RegsArm* arm = new RegsArm;
  arm->set_pc(0x2300);
  arm->set_sp(0x10000);
  reg_list.push_back(arm);

  RegsArm64* arm64 = new RegsArm64;
  arm64->set_pc(0x2300);
  arm64->set_sp(0x10000);
  reg_list.push_back(arm64);

  RegsX86* x86 = new RegsX86;
  x86->set_pc(0x2300);
  x86->set_sp(0x10000);
  reg_list.push_back(x86);

  RegsX86_64* x86_64 = new RegsX86_64;
  x86_64->set_pc(0x2300);
  x86_64->set_sp(0x10000);
  reg_list.push_back(x86_64);

  RegsMips* mips = new RegsMips;
  mips->set_pc(0x2300);
  mips->set_sp(0x10000);
  reg_list.push_back(mips);

  RegsMips64* mips64 = new RegsMips64;
  mips64->set_pc(0x2300);
  mips64->set_sp(0x10000);
  reg_list.push_back(mips64);

  for (auto regs : reg_list) {
    ElfInterfaceFake::FakePushFunctionData(FunctionData("Frame0", 10));

    Unwinder unwinder(64, &maps_, regs, process_memory_);
    unwinder.Unwind();

    ASSERT_EQ(1U, unwinder.NumFrames());
    std::string expected;
    switch (regs->Arch()) {
      case ARCH_ARM:
      case ARCH_X86:
      case ARCH_MIPS:
        expected = "  #00 pc 00001300  /system/fake/libc.so (Frame0+10)";
        break;
      case ARCH_ARM64:
      case ARCH_X86_64:
      case ARCH_MIPS64:
        expected = "  #00 pc 0000000000001300  /system/fake/libc.so (Frame0+10)";
        break;
      default:
        expected = "";
    }
    EXPECT_EQ(expected, unwinder.FormatFrame(0))
        << "Mismatch of frame format for regs arch " << ArchToString(regs->Arch());
    delete regs;
  }
}

}  // namespace unwindstack
