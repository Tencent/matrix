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

#include <stdint.h>

#include <gtest/gtest.h>

#include <unwindstack/Regs.h>

#include "RegsFake.h"
#include "RegsInfo.h"

namespace unwindstack {

TEST(RegsInfoTest, single_uint32_t) {
  RegsImplFake<uint32_t> regs(10);
  RegsInfo<uint32_t> info(&regs);

  regs[1] = 0x100;
  ASSERT_FALSE(info.IsSaved(1));
  ASSERT_EQ(0x100U, info.Get(1));
  ASSERT_EQ(10, info.Total());

  uint32_t* value = info.Save(1);
  ASSERT_EQ(value, &regs[1]);
  regs[1] = 0x200;
  ASSERT_TRUE(info.IsSaved(1));
  ASSERT_EQ(0x100U, info.Get(1));
  ASSERT_EQ(0x200U, regs[1]);
}

TEST(RegsInfoTest, single_uint64_t) {
  RegsImplFake<uint64_t> regs(20);
  RegsInfo<uint64_t> info(&regs);

  regs[3] = 0x300;
  ASSERT_FALSE(info.IsSaved(3));
  ASSERT_EQ(0x300U, info.Get(3));
  ASSERT_EQ(20, info.Total());

  uint64_t* value = info.Save(3);
  ASSERT_EQ(value, &regs[3]);
  regs[3] = 0x400;
  ASSERT_TRUE(info.IsSaved(3));
  ASSERT_EQ(0x300U, info.Get(3));
  ASSERT_EQ(0x400U, regs[3]);
}

TEST(RegsInfoTest, all) {
  RegsImplFake<uint64_t> regs(64);
  RegsInfo<uint64_t> info(&regs);

  for (uint32_t i = 0; i < 64; i++) {
    regs[i] = i * 0x100;
    ASSERT_EQ(i * 0x100, info.Get(i)) << "Reg " + std::to_string(i) + " failed.";
  }

  for (uint32_t i = 0; i < 64; i++) {
    ASSERT_FALSE(info.IsSaved(i)) << "Reg " + std::to_string(i) + " failed.";
    uint64_t* reg = info.Save(i);
    ASSERT_EQ(reg, &regs[i]) << "Reg " + std::to_string(i) + " failed.";
    *reg = i * 0x1000 + 0x100;
    ASSERT_EQ(i * 0x1000 + 0x100, regs[i]) << "Reg " + std::to_string(i) + " failed.";
  }

  for (uint32_t i = 0; i < 64; i++) {
    ASSERT_TRUE(info.IsSaved(i)) << "Reg " + std::to_string(i) + " failed.";
    ASSERT_EQ(i * 0x100, info.Get(i)) << "Reg " + std::to_string(i) + " failed.";
  }
}

TEST(RegsInfoTest, invalid_register) {
  RegsImplFake<uint64_t> regs(64);
  RegsInfo<uint64_t> info(&regs);

  EXPECT_DEATH(info.Save(RegsInfo<uint64_t>::MAX_REGISTERS), "");
}

}  // namespace unwindstack
