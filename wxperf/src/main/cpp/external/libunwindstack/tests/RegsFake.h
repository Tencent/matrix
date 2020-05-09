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

#ifndef _LIBUNWINDSTACK_TESTS_REGS_FAKE_H
#define _LIBUNWINDSTACK_TESTS_REGS_FAKE_H

#include <stdint.h>

#include <unwindstack/Elf.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>

#include "Check.h"

namespace unwindstack {

class RegsFake : public Regs {
 public:
  RegsFake(uint16_t total_regs) : Regs(total_regs, Regs::Location(Regs::LOCATION_UNKNOWN, 0)) {}
  virtual ~RegsFake() = default;

  ArchEnum Arch() override { return fake_arch_; }
  void* RawData() override { return nullptr; }
  uint64_t pc() override { return fake_pc_; }
  uint64_t sp() override { return fake_sp_; }
  void set_pc(uint64_t pc) override { fake_pc_ = pc; }
  void set_sp(uint64_t sp) override { fake_sp_ = sp; }

  bool SetPcFromReturnAddress(Memory*) override {
    if (!fake_return_address_valid_) {
      return false;
    }
    fake_pc_ = fake_return_address_;
    return true;
  }

  void IterateRegisters(std::function<void(const char*, uint64_t)>) override {}

  bool Is32Bit() {
    CHECK(fake_arch_ != ARCH_UNKNOWN);
    return fake_arch_ == ARCH_ARM || fake_arch_ == ARCH_X86 || fake_arch_ == ARCH_MIPS;
  }

  uint64_t GetPcAdjustment(uint64_t, Elf*) override { return 2; }

  bool StepIfSignalHandler(uint64_t, Elf*, Memory*) override { return false; }

  void FakeSetArch(ArchEnum arch) { fake_arch_ = arch; }
  void FakeSetDexPc(uint64_t dex_pc) { dex_pc_ = dex_pc; }
  void FakeSetReturnAddress(uint64_t return_address) { fake_return_address_ = return_address; }
  void FakeSetReturnAddressValid(bool valid) { fake_return_address_valid_ = valid; }

  Regs* Clone() override { return nullptr; }

 private:
  ArchEnum fake_arch_ = ARCH_UNKNOWN;
  uint64_t fake_pc_ = 0;
  uint64_t fake_sp_ = 0;
  bool fake_return_address_valid_ = false;
  uint64_t fake_return_address_ = 0;
};

template <typename TypeParam>
class RegsImplFake : public RegsImpl<TypeParam> {
 public:
  RegsImplFake(uint16_t total_regs)
      : RegsImpl<TypeParam>(total_regs, Regs::Location(Regs::LOCATION_UNKNOWN, 0)) {}
  virtual ~RegsImplFake() = default;

  ArchEnum Arch() override { return ARCH_UNKNOWN; }
  uint64_t pc() override { return fake_pc_; }
  uint64_t sp() override { return fake_sp_; }
  void set_pc(uint64_t pc) override { fake_pc_ = pc; }
  void set_sp(uint64_t sp) override { fake_sp_ = sp; }

  uint64_t GetPcAdjustment(uint64_t, Elf*) override { return 0; }
  bool SetPcFromReturnAddress(Memory*) override { return false; }
  bool StepIfSignalHandler(uint64_t, Elf*, Memory*) override { return false; }

  Regs* Clone() override { return nullptr; }

 private:
  uint64_t fake_pc_ = 0;
  uint64_t fake_sp_ = 0;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_TESTS_REGS_FAKE_H
