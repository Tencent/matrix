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

#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>

#include "ElfFake.h"

namespace unwindstack {

TEST(MapInfoTest, maps_constructor_const_char) {
  MapInfo prev_map(nullptr, 0, 0, 0, 0, "");
  MapInfo map_info(&prev_map, 1, 2, 3, 4, "map");

  EXPECT_EQ(&prev_map, map_info.prev_map);
  EXPECT_EQ(1UL, map_info.start);
  EXPECT_EQ(2UL, map_info.end);
  EXPECT_EQ(3UL, map_info.offset);
  EXPECT_EQ(4UL, map_info.flags);
  EXPECT_EQ("map", map_info.name);
  EXPECT_EQ(static_cast<uint64_t>(-1), map_info.load_bias);
  EXPECT_EQ(0UL, map_info.elf_offset);
  EXPECT_TRUE(map_info.elf.get() == nullptr);
}

TEST(MapInfoTest, maps_constructor_string) {
  std::string name("string_map");
  MapInfo prev_map(nullptr, 0, 0, 0, 0, "");
  MapInfo map_info(&prev_map, 1, 2, 3, 4, name);

  EXPECT_EQ(&prev_map, map_info.prev_map);
  EXPECT_EQ(1UL, map_info.start);
  EXPECT_EQ(2UL, map_info.end);
  EXPECT_EQ(3UL, map_info.offset);
  EXPECT_EQ(4UL, map_info.flags);
  EXPECT_EQ("string_map", map_info.name);
  EXPECT_EQ(static_cast<uint64_t>(-1), map_info.load_bias);
  EXPECT_EQ(0UL, map_info.elf_offset);
  EXPECT_TRUE(map_info.elf.get() == nullptr);
}

TEST(MapInfoTest, get_function_name) {
  ElfFake* elf = new ElfFake(nullptr);
  ElfInterfaceFake* interface = new ElfInterfaceFake(nullptr);
  elf->FakeSetInterface(interface);
  interface->FakePushFunctionData(FunctionData("function", 1000));

  MapInfo map_info(nullptr, 1, 2, 3, 4, "");
  map_info.elf.reset(elf);

  std::string name;
  uint64_t offset;
  ASSERT_TRUE(map_info.GetFunctionName(1000, &name, &offset));
  EXPECT_EQ("function", name);
  EXPECT_EQ(1000UL, offset);
}

}  // namespace unwindstack
