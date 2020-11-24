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

#include <stdint.h>

#include <deque>
#include <ios>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <unwindstack/Log.h>
#include <unwindstack/RegsArm.h>

#include "ArmExidx.h"

#include "LogFake.h"
#include "MemoryFake.h"

namespace unwindstack {

class ArmExidxDecodeTest : public ::testing::TestWithParam<std::string> {
 protected:
  void Init(Memory* process_memory = nullptr) {
    if (process_memory == nullptr) {
      process_memory = &process_memory_;
    }

    regs_arm_.reset(new RegsArm());
    for (size_t i = 0; i < regs_arm_->total_regs(); i++) {
      (*regs_arm_)[i] = 0;
    }
    regs_arm_->set_pc(0);
    regs_arm_->set_sp(0);

    exidx_.reset(new ArmExidx(regs_arm_.get(), &elf_memory_, process_memory));
    if (log_ != ARM_LOG_NONE) {
      exidx_->set_log(log_);
      exidx_->set_log_indent(0);
      exidx_->set_log_skip_execution(false);
    }
    data_ = exidx_->data();
    exidx_->set_cfa(0x10000);
  }

  void SetUp() override {
    if (GetParam() == "no_logging") {
      log_ = ARM_LOG_NONE;
    } else if (GetParam() == "register_logging") {
      log_ = ARM_LOG_BY_REG;
    } else {
      log_ = ARM_LOG_FULL;
    }
    elf_memory_.Clear();
    process_memory_.Clear();
    ResetExidx();
  }

  void ResetExidx() {
    ResetLogs();
    Init();
  }

  std::unique_ptr<ArmExidx> exidx_;
  std::unique_ptr<RegsArm> regs_arm_;
  std::deque<uint8_t>* data_;

  MemoryFake elf_memory_;
  MemoryFake process_memory_;
  ArmLogType log_;
};

TEST_P(ArmExidxDecodeTest, vsp_incr) {
  // 00xxxxxx: vsp = vsp + (xxxxxx << 2) + 4
  data_->push_back(0x00);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp + 4\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 + 4\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10004U, exidx_->cfa());

  ResetExidx();
  data_->clear();
  data_->push_back(0x01);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp + 8\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 + 8\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());

  ResetExidx();
  data_->clear();
  data_->push_back(0x3f);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp + 256\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 + 256\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10100U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, vsp_decr) {
  // 01xxxxxx: vsp = vsp - (xxxxxx << 2) + 4
  data_->push_back(0x40);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp - 4\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 - 4\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0xfffcU, exidx_->cfa());

  ResetExidx();
  data_->clear();
  data_->push_back(0x41);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp - 8\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 - 8\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0xfff8U, exidx_->cfa());

  ResetExidx();
  data_->clear();
  data_->push_back(0x7f);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp - 256\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 - 256\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0xff00U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, refuse_unwind) {
  // 10000000 00000000: Refuse to unwind
  data_->push_back(0x80);
  data_->push_back(0x00);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Refuse to unwind\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(ARM_STATUS_NO_UNWIND, exidx_->status());
}

TEST_P(ArmExidxDecodeTest, pop_up_to_12) {
  // 1000iiii iiiiiiii: Pop up to 12 integer registers
  data_->push_back(0x88);
  data_->push_back(0x00);
  process_memory_.SetData32(0x10000, 0x10);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_TRUE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r15}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 4\n"
          "4 unwind r15 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10004U, exidx_->cfa());
  ASSERT_EQ(0x10U, (*exidx_->regs())[15]);

  ResetExidx();
  data_->push_back(0x8f);
  data_->push_back(0xff);
  for (size_t i = 0; i < 12; i++) {
    process_memory_.SetData32(0x10000 + i * 4, i + 0x20);
  }
  exidx_->set_pc_set(false);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_TRUE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15}\n",
                GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 48\n"
          "4 unwind r4 = [cfa - 48]\n"
          "4 unwind r5 = [cfa - 44]\n"
          "4 unwind r6 = [cfa - 40]\n"
          "4 unwind r7 = [cfa - 36]\n"
          "4 unwind r8 = [cfa - 32]\n"
          "4 unwind r9 = [cfa - 28]\n"
          "4 unwind r10 = [cfa - 24]\n"
          "4 unwind r11 = [cfa - 20]\n"
          "4 unwind r12 = [cfa - 16]\n"
          "4 unwind r13 = [cfa - 12]\n"
          "4 unwind r14 = [cfa - 8]\n"
          "4 unwind r15 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  // Popping r13 results in a modified cfa.
  ASSERT_EQ(0x29U, exidx_->cfa());

  ASSERT_EQ(0x20U, (*exidx_->regs())[4]);
  ASSERT_EQ(0x21U, (*exidx_->regs())[5]);
  ASSERT_EQ(0x22U, (*exidx_->regs())[6]);
  ASSERT_EQ(0x23U, (*exidx_->regs())[7]);
  ASSERT_EQ(0x24U, (*exidx_->regs())[8]);
  ASSERT_EQ(0x25U, (*exidx_->regs())[9]);
  ASSERT_EQ(0x26U, (*exidx_->regs())[10]);
  ASSERT_EQ(0x27U, (*exidx_->regs())[11]);
  ASSERT_EQ(0x28U, (*exidx_->regs())[12]);
  ASSERT_EQ(0x29U, (*exidx_->regs())[13]);
  ASSERT_EQ(0x2aU, (*exidx_->regs())[14]);
  ASSERT_EQ(0x2bU, (*exidx_->regs())[15]);

  ResetExidx();
  exidx_->set_cfa(0x10034);
  data_->push_back(0x81);
  data_->push_back(0x28);
  process_memory_.SetData32(0x10034, 0x11);
  process_memory_.SetData32(0x10038, 0x22);
  process_memory_.SetData32(0x1003c, 0x33);
  exidx_->set_pc_set(false);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r7, r9, r12}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 12\n"
          "4 unwind r7 = [cfa - 12]\n"
          "4 unwind r9 = [cfa - 8]\n"
          "4 unwind r12 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10040U, exidx_->cfa());
  ASSERT_EQ(0x11U, (*exidx_->regs())[7]);
  ASSERT_EQ(0x22U, (*exidx_->regs())[9]);
  ASSERT_EQ(0x33U, (*exidx_->regs())[12]);
}

TEST_P(ArmExidxDecodeTest, set_vsp_from_register) {
  // 1001nnnn: Set vsp = r[nnnn] (nnnn != 13, 15)
  exidx_->set_cfa(0x100);
  for (size_t i = 0; i < 15; i++) {
    (*regs_arm_)[i] = i + 1;
  }

  data_->push_back(0x90);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = r0\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r0\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(1U, exidx_->cfa());

  ResetExidx();
  exidx_->set_cfa(0x100);
  for (size_t i = 0; i < 15; i++) {
    (*regs_arm_)[i] = i + 1;
  }
  data_->push_back(0x93);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = r3\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r3\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(4U, exidx_->cfa());

  ResetExidx();
  exidx_->set_cfa(0x100);
  for (size_t i = 0; i < 15; i++) {
    (*regs_arm_)[i] = i + 1;
  }
  data_->push_back(0x9e);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = r14\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r14\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(15U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, reserved_prefix) {
  // 10011101: Reserved as prefix for ARM register to register moves
  data_->push_back(0x9d);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind [Reserved]\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(ARM_STATUS_RESERVED, exidx_->status());

  // 10011111: Reserved as prefix for Intel Wireless MMX register to register moves
  ResetExidx();
  data_->push_back(0x9f);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind [Reserved]\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(ARM_STATUS_RESERVED, exidx_->status());
}

TEST_P(ArmExidxDecodeTest, pop_registers) {
  // 10100nnn: Pop r4-r[4+nnn]
  data_->push_back(0xa0);
  process_memory_.SetData32(0x10000, 0x14);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r4}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 4\n"
          "4 unwind r4 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10004U, exidx_->cfa());
  ASSERT_EQ(0x14U, (*exidx_->regs())[4]);

  ResetExidx();
  data_->push_back(0xa3);
  process_memory_.SetData32(0x10000, 0x20);
  process_memory_.SetData32(0x10004, 0x30);
  process_memory_.SetData32(0x10008, 0x40);
  process_memory_.SetData32(0x1000c, 0x50);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r4-r7}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 16\n"
          "4 unwind r4 = [cfa - 16]\n"
          "4 unwind r5 = [cfa - 12]\n"
          "4 unwind r6 = [cfa - 8]\n"
          "4 unwind r7 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10010U, exidx_->cfa());
  ASSERT_EQ(0x20U, (*exidx_->regs())[4]);
  ASSERT_EQ(0x30U, (*exidx_->regs())[5]);
  ASSERT_EQ(0x40U, (*exidx_->regs())[6]);
  ASSERT_EQ(0x50U, (*exidx_->regs())[7]);

  ResetExidx();
  data_->push_back(0xa7);
  process_memory_.SetData32(0x10000, 0x41);
  process_memory_.SetData32(0x10004, 0x51);
  process_memory_.SetData32(0x10008, 0x61);
  process_memory_.SetData32(0x1000c, 0x71);
  process_memory_.SetData32(0x10010, 0x81);
  process_memory_.SetData32(0x10014, 0x91);
  process_memory_.SetData32(0x10018, 0xa1);
  process_memory_.SetData32(0x1001c, 0xb1);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r4-r11}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 32\n"
          "4 unwind r4 = [cfa - 32]\n"
          "4 unwind r5 = [cfa - 28]\n"
          "4 unwind r6 = [cfa - 24]\n"
          "4 unwind r7 = [cfa - 20]\n"
          "4 unwind r8 = [cfa - 16]\n"
          "4 unwind r9 = [cfa - 12]\n"
          "4 unwind r10 = [cfa - 8]\n"
          "4 unwind r11 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10020U, exidx_->cfa());
  ASSERT_EQ(0x41U, (*exidx_->regs())[4]);
  ASSERT_EQ(0x51U, (*exidx_->regs())[5]);
  ASSERT_EQ(0x61U, (*exidx_->regs())[6]);
  ASSERT_EQ(0x71U, (*exidx_->regs())[7]);
  ASSERT_EQ(0x81U, (*exidx_->regs())[8]);
  ASSERT_EQ(0x91U, (*exidx_->regs())[9]);
  ASSERT_EQ(0xa1U, (*exidx_->regs())[10]);
  ASSERT_EQ(0xb1U, (*exidx_->regs())[11]);
}

TEST_P(ArmExidxDecodeTest, pop_registers_with_r14) {
  // 10101nnn: Pop r4-r[4+nnn], r14
  data_->push_back(0xa8);
  process_memory_.SetData32(0x10000, 0x12);
  process_memory_.SetData32(0x10004, 0x22);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r4, r14}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 8\n"
          "4 unwind r4 = [cfa - 8]\n"
          "4 unwind r14 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());
  ASSERT_EQ(0x12U, (*exidx_->regs())[4]);
  ASSERT_EQ(0x22U, (*exidx_->regs())[14]);

  ResetExidx();
  data_->push_back(0xab);
  process_memory_.SetData32(0x10000, 0x1);
  process_memory_.SetData32(0x10004, 0x2);
  process_memory_.SetData32(0x10008, 0x3);
  process_memory_.SetData32(0x1000c, 0x4);
  process_memory_.SetData32(0x10010, 0x5);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r4-r7, r14}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 20\n"
          "4 unwind r4 = [cfa - 20]\n"
          "4 unwind r5 = [cfa - 16]\n"
          "4 unwind r6 = [cfa - 12]\n"
          "4 unwind r7 = [cfa - 8]\n"
          "4 unwind r14 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10014U, exidx_->cfa());
  ASSERT_EQ(0x1U, (*exidx_->regs())[4]);
  ASSERT_EQ(0x2U, (*exidx_->regs())[5]);
  ASSERT_EQ(0x3U, (*exidx_->regs())[6]);
  ASSERT_EQ(0x4U, (*exidx_->regs())[7]);
  ASSERT_EQ(0x5U, (*exidx_->regs())[14]);

  ResetExidx();
  data_->push_back(0xaf);
  process_memory_.SetData32(0x10000, 0x1a);
  process_memory_.SetData32(0x10004, 0x2a);
  process_memory_.SetData32(0x10008, 0x3a);
  process_memory_.SetData32(0x1000c, 0x4a);
  process_memory_.SetData32(0x10010, 0x5a);
  process_memory_.SetData32(0x10014, 0x6a);
  process_memory_.SetData32(0x10018, 0x7a);
  process_memory_.SetData32(0x1001c, 0x8a);
  process_memory_.SetData32(0x10020, 0x9a);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r4-r11, r14}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 36\n"
          "4 unwind r4 = [cfa - 36]\n"
          "4 unwind r5 = [cfa - 32]\n"
          "4 unwind r6 = [cfa - 28]\n"
          "4 unwind r7 = [cfa - 24]\n"
          "4 unwind r8 = [cfa - 20]\n"
          "4 unwind r9 = [cfa - 16]\n"
          "4 unwind r10 = [cfa - 12]\n"
          "4 unwind r11 = [cfa - 8]\n"
          "4 unwind r14 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10024U, exidx_->cfa());
  ASSERT_EQ(0x1aU, (*exidx_->regs())[4]);
  ASSERT_EQ(0x2aU, (*exidx_->regs())[5]);
  ASSERT_EQ(0x3aU, (*exidx_->regs())[6]);
  ASSERT_EQ(0x4aU, (*exidx_->regs())[7]);
  ASSERT_EQ(0x5aU, (*exidx_->regs())[8]);
  ASSERT_EQ(0x6aU, (*exidx_->regs())[9]);
  ASSERT_EQ(0x7aU, (*exidx_->regs())[10]);
  ASSERT_EQ(0x8aU, (*exidx_->regs())[11]);
  ASSERT_EQ(0x9aU, (*exidx_->regs())[14]);
}

TEST_P(ArmExidxDecodeTest, finish) {
  // 10110000: Finish
  data_->push_back(0xb0);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind finish\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10000U, exidx_->cfa());
  ASSERT_EQ(ARM_STATUS_FINISH, exidx_->status());
}

TEST_P(ArmExidxDecodeTest, spare) {
  // 10110001 00000000: Spare
  data_->push_back(0xb1);
  data_->push_back(0x00);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Spare\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10000U, exidx_->cfa());
  ASSERT_EQ(ARM_STATUS_SPARE, exidx_->status());

  // 10110001 xxxxyyyy: Spare (xxxx != 0000)
  for (size_t x = 1; x < 16; x++) {
    for (size_t y = 0; y < 16; y++) {
      ResetExidx();
      data_->push_back(0xb1);
      data_->push_back((x << 4) | y);
      ASSERT_FALSE(exidx_->Decode()) << "x, y = " << x << ", " << y;
      ASSERT_EQ("", GetFakeLogBuf()) << "x, y = " << x << ", " << y;
      switch (log_) {
        case ARM_LOG_NONE:
          ASSERT_EQ("", GetFakeLogPrint());
          break;
        case ARM_LOG_FULL:
        case ARM_LOG_BY_REG:
          ASSERT_EQ("4 unwind Spare\n", GetFakeLogPrint()) << "x, y = " << x << ", " << y;
          break;
      }
      ASSERT_EQ(0x10000U, exidx_->cfa()) << "x, y = " << x << ", " << y;
      ASSERT_EQ(ARM_STATUS_SPARE, exidx_->status());
    }
  }

  // 101101nn: Spare
  for (size_t n = 0; n < 4; n++) {
    ResetExidx();
    data_->push_back(0xb4 | n);
    ASSERT_FALSE(exidx_->Decode()) << "n = " << n;
    ASSERT_EQ("", GetFakeLogBuf()) << "n = " << n;
    switch (log_) {
      case ARM_LOG_NONE:
        ASSERT_EQ("", GetFakeLogPrint());
        break;
      case ARM_LOG_FULL:
      case ARM_LOG_BY_REG:
        ASSERT_EQ("4 unwind Spare\n", GetFakeLogPrint()) << "n = " << n;
        break;
    }
    ASSERT_EQ(0x10000U, exidx_->cfa()) << "n = " << n;
    ASSERT_EQ(ARM_STATUS_SPARE, exidx_->status());
  }

  // 11000111 00000000: Spare
  ResetExidx();
  data_->push_back(0xc7);
  data_->push_back(0x00);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Spare\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10000U, exidx_->cfa());
  ASSERT_EQ(ARM_STATUS_SPARE, exidx_->status());

  // 11000111 xxxxyyyy: Spare (xxxx != 0000)
  for (size_t x = 1; x < 16; x++) {
    for (size_t y = 0; y < 16; y++) {
      ResetExidx();
      data_->push_back(0xc7);
      data_->push_back(0x10);
      ASSERT_FALSE(exidx_->Decode()) << "x, y = " << x << ", " << y;
      ASSERT_EQ("", GetFakeLogBuf()) << "x, y = " << x << ", " << y;
      switch (log_) {
        case ARM_LOG_NONE:
          ASSERT_EQ("", GetFakeLogPrint());
          break;
        case ARM_LOG_FULL:
        case ARM_LOG_BY_REG:
          ASSERT_EQ("4 unwind Spare\n", GetFakeLogPrint()) << "x, y = " << x << ", " << y;
          break;
      }
      ASSERT_EQ(0x10000U, exidx_->cfa()) << "x, y = " << x << ", " << y;
      ASSERT_EQ(ARM_STATUS_SPARE, exidx_->status());
    }
  }

  // 11001yyy: Spare (yyy != 000, 001)
  for (size_t y = 2; y < 8; y++) {
    ResetExidx();
    data_->push_back(0xc8 | y);
    ASSERT_FALSE(exidx_->Decode()) << "y = " << y;
    ASSERT_EQ("", GetFakeLogBuf()) << "y = " << y;
    switch (log_) {
      case ARM_LOG_NONE:
        ASSERT_EQ("", GetFakeLogPrint());
        break;
      case ARM_LOG_FULL:
      case ARM_LOG_BY_REG:
        ASSERT_EQ("4 unwind Spare\n", GetFakeLogPrint()) << "y = " << y;
        break;
    }
    ASSERT_EQ(0x10000U, exidx_->cfa()) << "y = " << y;
    ASSERT_EQ(ARM_STATUS_SPARE, exidx_->status());
  }

  // 11xxxyyy: Spare (xxx != 000, 001, 010)
  for (size_t x = 3; x < 8; x++) {
    for (size_t y = 0; y < 8; y++) {
      ResetExidx();
      data_->push_back(0xc0 | (x << 3) | y);
      ASSERT_FALSE(exidx_->Decode()) << "x, y = " << x << ", " << y;
      ASSERT_EQ("", GetFakeLogBuf()) << "x, y = " << x << ", " << y;
      switch (log_) {
        case ARM_LOG_NONE:
          ASSERT_EQ("", GetFakeLogPrint());
          break;
        case ARM_LOG_FULL:
        case ARM_LOG_BY_REG:
          ASSERT_EQ("4 unwind Spare\n", GetFakeLogPrint()) << "x, y = " << x << ", " << y;
          break;
      }
      ASSERT_EQ(0x10000U, exidx_->cfa()) << "x, y = " << x << ", " << y;
      ASSERT_EQ(ARM_STATUS_SPARE, exidx_->status());
    }
  }
}

TEST_P(ArmExidxDecodeTest, pop_registers_under_mask) {
  // 10110001 0000iiii: Pop integer registers {r0, r1, r2, r3}
  data_->push_back(0xb1);
  data_->push_back(0x01);
  process_memory_.SetData32(0x10000, 0x45);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r0}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 4\n"
          "4 unwind r0 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10004U, exidx_->cfa());
  ASSERT_EQ(0x45U, (*exidx_->regs())[0]);

  ResetExidx();
  data_->push_back(0xb1);
  data_->push_back(0x0a);
  process_memory_.SetData32(0x10000, 0x23);
  process_memory_.SetData32(0x10004, 0x24);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r1, r3}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 8\n"
          "4 unwind r1 = [cfa - 8]\n"
          "4 unwind r3 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());
  ASSERT_EQ(0x23U, (*exidx_->regs())[1]);
  ASSERT_EQ(0x24U, (*exidx_->regs())[3]);

  ResetExidx();
  data_->push_back(0xb1);
  data_->push_back(0x0f);
  process_memory_.SetData32(0x10000, 0x65);
  process_memory_.SetData32(0x10004, 0x54);
  process_memory_.SetData32(0x10008, 0x43);
  process_memory_.SetData32(0x1000c, 0x32);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {r0, r1, r2, r3}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 16\n"
          "4 unwind r0 = [cfa - 16]\n"
          "4 unwind r1 = [cfa - 12]\n"
          "4 unwind r2 = [cfa - 8]\n"
          "4 unwind r3 = [cfa - 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10010U, exidx_->cfa());
  ASSERT_EQ(0x65U, (*exidx_->regs())[0]);
  ASSERT_EQ(0x54U, (*exidx_->regs())[1]);
  ASSERT_EQ(0x43U, (*exidx_->regs())[2]);
  ASSERT_EQ(0x32U, (*exidx_->regs())[3]);
}

TEST_P(ArmExidxDecodeTest, vsp_large_incr) {
  // 10110010 uleb128: vsp = vsp + 0x204 + (uleb128 << 2)
  data_->push_back(0xb2);
  data_->push_back(0x7f);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp + 1024\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 + 1024\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10400U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xb2);
  data_->push_back(0xff);
  data_->push_back(0x02);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp + 2048\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 + 2048\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10800U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xb2);
  data_->push_back(0xff);
  data_->push_back(0x82);
  data_->push_back(0x30);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind vsp = vsp + 3147776\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 + 3147776\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x310800U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, pop_vfp_fstmfdx) {
  // 10110011 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] by FSTMFDX
  data_->push_back(0xb3);
  data_->push_back(0x00);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d0}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x1000cU, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xb3);
  data_->push_back(0x48);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d4-d12}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x1004cU, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, pop_vfp8_fstmfdx) {
  // 10111nnn: Pop VFP double precision registers D[8]-D[8+nnn] by FSTMFDX
  data_->push_back(0xb8);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d8}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x1000cU, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xbb);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d8-d11}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10024U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xbf);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d8-d15}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10044U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, pop_mmx_wr10) {
  // 11000nnn: Intel Wireless MMX pop wR[10]-wR[10+nnn] (nnn != 6, 7)
  data_->push_back(0xc0);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wR10}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wRX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc2);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wR10-wR12}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wRX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10018U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc5);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wR10-wR15}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wRX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10030U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, pop_mmx_wr) {
  // 11000110 sssscccc: Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc]
  data_->push_back(0xc6);
  data_->push_back(0x00);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wR0}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wRX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc6);
  data_->push_back(0x25);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wR2-wR7}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wRX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10030U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc6);
  data_->push_back(0xff);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wR15-wR30}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wRX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10080U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, pop_mmx_wcgr) {
  // 11000111 0000iiii: Intel Wireless MMX pop wCGR registes {wCGR0,1,2,3}
  data_->push_back(0xc7);
  data_->push_back(0x01);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wCGR0}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wCGR register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10004U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc7);
  data_->push_back(0x0a);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wCGR1, wCGR3}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wCGR register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc7);
  data_->push_back(0x0f);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {wCGR0, wCGR1, wCGR2, wCGR3}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported wCGR register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10010U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, pop_vfp16_vpush) {
  // 11001000 sssscccc: Pop VFP double precision registers d[16+ssss]-D[16+ssss+cccc] by VPUSH
  data_->push_back(0xc8);
  data_->push_back(0x00);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d16}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc8);
  data_->push_back(0x14);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d17-d21}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10028U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc8);
  data_->push_back(0xff);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d31-d46}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10080U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, pop_vfp_vpush) {
  // 11001001 sssscccc: Pop VFP double precision registers d[ssss]-D[ssss+cccc] by VPUSH
  data_->push_back(0xc9);
  data_->push_back(0x00);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d0}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc9);
  data_->push_back(0x23);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d2-d5}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10020U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xc9);
  data_->push_back(0xff);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d15-d30}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10080U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, pop_vfp8_vpush) {
  // 11010nnn: Pop VFP double precision registers D[8]-D[8+nnn] by VPUSH
  data_->push_back(0xd0);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d8}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10008U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xd2);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d8-d10}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10018U, exidx_->cfa());

  ResetExidx();
  data_->push_back(0xd7);
  ASSERT_TRUE(exidx_->Decode());
  ASSERT_FALSE(exidx_->pc_set());
  ASSERT_EQ("", GetFakeLogBuf());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ("4 unwind pop {d8-d15}\n", GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      ASSERT_EQ("4 unwind Unsupported DX register display\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10040U, exidx_->cfa());
}

TEST_P(ArmExidxDecodeTest, expect_truncated) {
  // This test verifies that any op that requires extra ops will
  // fail if the data is not present.
  data_->push_back(0x80);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ(ARM_STATUS_TRUNCATED, exidx_->status());

  data_->clear();
  data_->push_back(0xb1);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ(ARM_STATUS_TRUNCATED, exidx_->status());

  data_->clear();
  data_->push_back(0xb2);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ(ARM_STATUS_TRUNCATED, exidx_->status());

  data_->clear();
  data_->push_back(0xb3);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ(ARM_STATUS_TRUNCATED, exidx_->status());

  data_->clear();
  data_->push_back(0xc6);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ(ARM_STATUS_TRUNCATED, exidx_->status());

  data_->clear();
  data_->push_back(0xc7);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ(ARM_STATUS_TRUNCATED, exidx_->status());

  data_->clear();
  data_->push_back(0xc8);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ(ARM_STATUS_TRUNCATED, exidx_->status());

  data_->clear();
  data_->push_back(0xc9);
  ASSERT_FALSE(exidx_->Decode());
  ASSERT_EQ(ARM_STATUS_TRUNCATED, exidx_->status());
}

TEST_P(ArmExidxDecodeTest, verify_no_truncated) {
  // This test verifies that no pattern results in a crash or truncation.
  MemoryFakeAlwaysReadZero memory_zero;
  Init(&memory_zero);

  for (size_t x = 0; x < 256; x++) {
    if (x == 0xb2) {
      // This opcode is followed by an uleb128, so just skip this one.
      continue;
    }
    for (size_t y = 0; y < 256; y++) {
      data_->clear();
      data_->push_back(x);
      data_->push_back(y);
      if (!exidx_->Decode()) {
        ASSERT_NE(ARM_STATUS_TRUNCATED, exidx_->status())
            << "x y = 0x" << std::hex << x << " 0x" << y;
        ASSERT_NE(ARM_STATUS_READ_FAILED, exidx_->status())
            << "x y = 0x" << std::hex << x << " 0x" << y;
      }
    }
  }
}

TEST_P(ArmExidxDecodeTest, eval_multiple_decodes) {
  // vsp = vsp + 4
  data_->push_back(0x00);
  // vsp = vsp + 12
  data_->push_back(0x02);
  // Finish
  data_->push_back(0xb0);

  ASSERT_TRUE(exidx_->Eval());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ(
          "4 unwind vsp = vsp + 4\n"
          "4 unwind vsp = vsp + 12\n"
          "4 unwind finish\n",
          GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ("4 unwind cfa = r13 + 16\n", GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10010U, exidx_->cfa());
  ASSERT_FALSE(exidx_->pc_set());
}

TEST_P(ArmExidxDecodeTest, eval_vsp_add_after_pop) {
  // Pop {r15}
  data_->push_back(0x88);
  data_->push_back(0x00);
  // vsp = vsp + 12
  data_->push_back(0x02);
  // Finish
  data_->push_back(0xb0);
  process_memory_.SetData32(0x10000, 0x10);

  ASSERT_TRUE(exidx_->Eval());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ(
          "4 unwind pop {r15}\n"
          "4 unwind vsp = vsp + 12\n"
          "4 unwind finish\n",
          GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 16\n"
          "4 unwind r15 = [cfa - 16]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10010U, exidx_->cfa());
  ASSERT_TRUE(exidx_->pc_set());
  ASSERT_EQ(0x10U, (*exidx_->regs())[15]);
}

TEST_P(ArmExidxDecodeTest, eval_vsp_add_large_after_pop) {
  // Pop {r15}
  data_->push_back(0x88);
  data_->push_back(0x00);
  // vsp = vsp + 1024
  data_->push_back(0xb2);
  data_->push_back(0x7f);
  // Finish
  data_->push_back(0xb0);
  process_memory_.SetData32(0x10000, 0x10);

  ASSERT_TRUE(exidx_->Eval());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ(
          "4 unwind pop {r15}\n"
          "4 unwind vsp = vsp + 1024\n"
          "4 unwind finish\n",
          GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 1028\n"
          "4 unwind r15 = [cfa - 1028]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10404U, exidx_->cfa());
  ASSERT_TRUE(exidx_->pc_set());
  ASSERT_EQ(0x10U, (*exidx_->regs())[15]);
}

TEST_P(ArmExidxDecodeTest, eval_vsp_sub_after_pop) {
  // Pop {r15}
  data_->push_back(0x88);
  data_->push_back(0x00);
  // vsp = vsp - 4
  data_->push_back(0x41);
  // Finish
  data_->push_back(0xb0);
  process_memory_.SetData32(0x10000, 0x10);

  ASSERT_TRUE(exidx_->Eval());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ(
          "4 unwind pop {r15}\n"
          "4 unwind vsp = vsp - 8\n"
          "4 unwind finish\n",
          GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 - 4\n"
          "4 unwind r15 = [cfa + 4]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0xfffcU, exidx_->cfa());
  ASSERT_TRUE(exidx_->pc_set());
  ASSERT_EQ(0x10U, (*exidx_->regs())[15]);
}

TEST_P(ArmExidxDecodeTest, eval_pc_set) {
  // vsp = vsp + 4
  data_->push_back(0x00);
  // vsp = vsp + 12
  data_->push_back(0x02);
  // Pop {r15}
  data_->push_back(0x88);
  data_->push_back(0x00);
  // vsp = vsp + 12
  data_->push_back(0x02);
  // Finish
  data_->push_back(0xb0);

  process_memory_.SetData32(0x10010, 0x10);

  ASSERT_TRUE(exidx_->Eval());
  switch (log_) {
    case ARM_LOG_NONE:
      ASSERT_EQ("", GetFakeLogPrint());
      break;
    case ARM_LOG_FULL:
      ASSERT_EQ(
          "4 unwind vsp = vsp + 4\n"
          "4 unwind vsp = vsp + 12\n"
          "4 unwind pop {r15}\n"
          "4 unwind vsp = vsp + 12\n"
          "4 unwind finish\n",
          GetFakeLogPrint());
      break;
    case ARM_LOG_BY_REG:
      exidx_->LogByReg();
      ASSERT_EQ(
          "4 unwind cfa = r13 + 32\n"
          "4 unwind r15 = [cfa - 16]\n",
          GetFakeLogPrint());
      break;
  }
  ASSERT_EQ(0x10020U, exidx_->cfa());
  ASSERT_TRUE(exidx_->pc_set());
  ASSERT_EQ(0x10U, (*exidx_->regs())[15]);
}

INSTANTIATE_TEST_CASE_P(, ArmExidxDecodeTest,
                        ::testing::Values("logging", "register_logging", "no_logging"));

}  // namespace unwindstack
