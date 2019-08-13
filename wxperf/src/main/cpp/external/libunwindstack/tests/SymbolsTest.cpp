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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <vector>

#include <android-base/file.h>
#include <android-base/test_utils.h>
#include <gtest/gtest.h>

#include <unwindstack/Memory.h>

#include "MemoryFake.h"
#include "Symbols.h"

namespace unwindstack {

template <typename TypeParam>
class SymbolsTest : public ::testing::Test {
 protected:
  void SetUp() override { memory_.Clear(); }

  void InitSym(TypeParam* sym, uint32_t st_value, uint32_t st_size, uint32_t st_name) {
    memset(sym, 0, sizeof(*sym));
    sym->st_info = STT_FUNC;
    sym->st_value = st_value;
    sym->st_size = st_size;
    sym->st_name = st_name;
    sym->st_shndx = SHN_COMMON;
  }

  MemoryFake memory_;
};
TYPED_TEST_CASE_P(SymbolsTest);

TYPED_TEST_P(SymbolsTest, function_bounds_check) {
  Symbols symbols(0x1000, sizeof(TypeParam), sizeof(TypeParam), 0x2000, 0x100);

  TypeParam sym;
  this->InitSym(&sym, 0x5000, 0x10, 0x40);
  uint64_t offset = 0x1000;
  this->memory_.SetMemory(offset, &sym, sizeof(sym));

  std::string fake_name("fake_function");
  this->memory_.SetMemory(0x2040, fake_name.c_str(), fake_name.size() + 1);

  std::string name;
  uint64_t func_offset;
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x5000, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("fake_function", name);
  ASSERT_EQ(0U, func_offset);

  name.clear();
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x500f, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("fake_function", name);
  ASSERT_EQ(0xfU, func_offset);

  // Check one before and one after the function.
  ASSERT_FALSE(symbols.GetName<TypeParam>(0x4fff, 0, &this->memory_, &name, &func_offset));
  ASSERT_FALSE(symbols.GetName<TypeParam>(0x5010, 0, &this->memory_, &name, &func_offset));
}

TYPED_TEST_P(SymbolsTest, no_symbol) {
  Symbols symbols(0x1000, sizeof(TypeParam), sizeof(TypeParam), 0x2000, 0x100);

  TypeParam sym;
  this->InitSym(&sym, 0x5000, 0x10, 0x40);
  uint64_t offset = 0x1000;
  this->memory_.SetMemory(offset, &sym, sizeof(sym));

  std::string fake_name("fake_function");
  this->memory_.SetMemory(0x2040, fake_name.c_str(), fake_name.size() + 1);

  // First verify that we can get the name.
  std::string name;
  uint64_t func_offset;
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x5000, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("fake_function", name);
  ASSERT_EQ(0U, func_offset);

  // Now modify the info field so it's no longer a function.
  sym.st_info = 0;
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  // Clear the cache to force the symbol data to be re-read.
  symbols.ClearCache();
  ASSERT_FALSE(symbols.GetName<TypeParam>(0x5000, 0, &this->memory_, &name, &func_offset));

  // Set the function back, and set the shndx to UNDEF.
  sym.st_info = STT_FUNC;
  sym.st_shndx = SHN_UNDEF;
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  // Clear the cache to force the symbol data to be re-read.
  symbols.ClearCache();
  ASSERT_FALSE(symbols.GetName<TypeParam>(0x5000, 0, &this->memory_, &name, &func_offset));
}

TYPED_TEST_P(SymbolsTest, multiple_entries) {
  Symbols symbols(0x1000, sizeof(TypeParam) * 3, sizeof(TypeParam), 0x2000, 0x500);

  TypeParam sym;
  uint64_t offset = 0x1000;
  std::string fake_name;

  this->InitSym(&sym, 0x5000, 0x10, 0x40);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  fake_name = "function_one";
  this->memory_.SetMemory(0x2040, fake_name.c_str(), fake_name.size() + 1);
  offset += sizeof(sym);

  this->InitSym(&sym, 0x3004, 0x200, 0x100);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  fake_name = "function_two";
  this->memory_.SetMemory(0x2100, fake_name.c_str(), fake_name.size() + 1);
  offset += sizeof(sym);

  this->InitSym(&sym, 0xa010, 0x20, 0x230);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  fake_name = "function_three";
  this->memory_.SetMemory(0x2230, fake_name.c_str(), fake_name.size() + 1);

  std::string name;
  uint64_t func_offset;
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x3005, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_two", name);
  ASSERT_EQ(1U, func_offset);

  name.clear();
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x5004, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_one", name);
  ASSERT_EQ(4U, func_offset);

  name.clear();
  ASSERT_TRUE(symbols.GetName<TypeParam>(0xa011, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_three", name);
  ASSERT_EQ(1U, func_offset);

  // Reget some of the others to verify getting one function name doesn't
  // affect any of the next calls.
  name.clear();
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x5008, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_one", name);
  ASSERT_EQ(8U, func_offset);

  name.clear();
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x3008, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_two", name);
  ASSERT_EQ(4U, func_offset);

  name.clear();
  ASSERT_TRUE(symbols.GetName<TypeParam>(0xa01a, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_three", name);
  ASSERT_EQ(0xaU, func_offset);
}

TYPED_TEST_P(SymbolsTest, multiple_entries_nonstandard_size) {
  uint64_t entry_size = sizeof(TypeParam) + 5;
  Symbols symbols(0x1000, entry_size * 3, entry_size, 0x2000, 0x500);

  TypeParam sym;
  uint64_t offset = 0x1000;
  std::string fake_name;

  this->InitSym(&sym, 0x5000, 0x10, 0x40);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  fake_name = "function_one";
  this->memory_.SetMemory(0x2040, fake_name.c_str(), fake_name.size() + 1);
  offset += entry_size;

  this->InitSym(&sym, 0x3004, 0x200, 0x100);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  fake_name = "function_two";
  this->memory_.SetMemory(0x2100, fake_name.c_str(), fake_name.size() + 1);
  offset += entry_size;

  this->InitSym(&sym, 0xa010, 0x20, 0x230);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  fake_name = "function_three";
  this->memory_.SetMemory(0x2230, fake_name.c_str(), fake_name.size() + 1);

  std::string name;
  uint64_t func_offset;
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x3005, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_two", name);
  ASSERT_EQ(1U, func_offset);

  name.clear();
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x5004, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_one", name);
  ASSERT_EQ(4U, func_offset);

  name.clear();
  ASSERT_TRUE(symbols.GetName<TypeParam>(0xa011, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function_three", name);
  ASSERT_EQ(1U, func_offset);
}

TYPED_TEST_P(SymbolsTest, load_bias) {
  Symbols symbols(0x1000, sizeof(TypeParam), sizeof(TypeParam), 0x2000, 0x100);

  TypeParam sym;
  this->InitSym(&sym, 0x5000, 0x10, 0x40);
  uint64_t offset = 0x1000;
  this->memory_.SetMemory(offset, &sym, sizeof(sym));

  std::string fake_name("fake_function");
  this->memory_.SetMemory(0x2040, fake_name.c_str(), fake_name.size() + 1);

  // Set a non-zero load_bias that should be a valid function offset.
  std::string name;
  uint64_t func_offset;
  ASSERT_TRUE(symbols.GetName<TypeParam>(0x5004, 0x1000, &this->memory_, &name, &func_offset));
  ASSERT_EQ("fake_function", name);
  ASSERT_EQ(4U, func_offset);

  // Set a flag that should cause the load_bias to be ignored.
  sym.st_shndx = SHN_ABS;
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  // Clear the cache to force the symbol data to be re-read.
  symbols.ClearCache();
  ASSERT_FALSE(symbols.GetName<TypeParam>(0x5004, 0x1000, &this->memory_, &name, &func_offset));
}

TYPED_TEST_P(SymbolsTest, symtab_value_out_of_bounds) {
  Symbols symbols_end_at_100(0x1000, sizeof(TypeParam) * 2, sizeof(TypeParam), 0x2000, 0x100);
  Symbols symbols_end_at_200(0x1000, sizeof(TypeParam) * 2, sizeof(TypeParam), 0x2000, 0x200);

  TypeParam sym;
  uint64_t offset = 0x1000;

  this->InitSym(&sym, 0x5000, 0x10, 0xfb);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  offset += sizeof(sym);

  this->InitSym(&sym, 0x3000, 0x10, 0x100);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));

  // Put the name across the end of the tab.
  std::string fake_name("fake_function");
  this->memory_.SetMemory(0x20fb, fake_name.c_str(), fake_name.size() + 1);

  std::string name;
  uint64_t func_offset;
  // Verify that we can get the function name properly for both entries.
  ASSERT_TRUE(symbols_end_at_200.GetName<TypeParam>(0x5000, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("fake_function", name);
  ASSERT_EQ(0U, func_offset);
  ASSERT_TRUE(symbols_end_at_200.GetName<TypeParam>(0x3000, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("function", name);
  ASSERT_EQ(0U, func_offset);

  // Now use the symbol table that ends at 0x100.
  ASSERT_FALSE(
      symbols_end_at_100.GetName<TypeParam>(0x5000, 0, &this->memory_, &name, &func_offset));
  ASSERT_FALSE(
      symbols_end_at_100.GetName<TypeParam>(0x3000, 0, &this->memory_, &name, &func_offset));
}

// Verify the entire func table is cached.
TYPED_TEST_P(SymbolsTest, symtab_read_cached) {
  Symbols symbols(0x1000, 3 * sizeof(TypeParam), sizeof(TypeParam), 0xa000, 0x1000);

  TypeParam sym;
  uint64_t offset = 0x1000;

  // Make sure that these entries are not in ascending order.
  this->InitSym(&sym, 0x5000, 0x10, 0x100);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  offset += sizeof(sym);

  this->InitSym(&sym, 0x2000, 0x300, 0x200);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  offset += sizeof(sym);

  this->InitSym(&sym, 0x1000, 0x100, 0x300);
  this->memory_.SetMemory(offset, &sym, sizeof(sym));
  offset += sizeof(sym);

  // Do call that should cache all of the entries (except the string data).
  std::string name;
  uint64_t func_offset;
  ASSERT_FALSE(symbols.GetName<TypeParam>(0x6000, 0, &this->memory_, &name, &func_offset));
  this->memory_.Clear();
  ASSERT_FALSE(symbols.GetName<TypeParam>(0x6000, 0, &this->memory_, &name, &func_offset));

  // Clear the memory and only put the symbol data string data in memory.
  this->memory_.Clear();

  std::string fake_name;
  fake_name = "first_entry";
  this->memory_.SetMemory(0xa100, fake_name.c_str(), fake_name.size() + 1);
  fake_name = "second_entry";
  this->memory_.SetMemory(0xa200, fake_name.c_str(), fake_name.size() + 1);
  fake_name = "third_entry";
  this->memory_.SetMemory(0xa300, fake_name.c_str(), fake_name.size() + 1);

  ASSERT_TRUE(symbols.GetName<TypeParam>(0x5001, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("first_entry", name);
  ASSERT_EQ(1U, func_offset);

  ASSERT_TRUE(symbols.GetName<TypeParam>(0x2002, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("second_entry", name);
  ASSERT_EQ(2U, func_offset);

  ASSERT_TRUE(symbols.GetName<TypeParam>(0x1003, 0, &this->memory_, &name, &func_offset));
  ASSERT_EQ("third_entry", name);
  ASSERT_EQ(3U, func_offset);
}

TYPED_TEST_P(SymbolsTest, get_global) {
  uint64_t start_offset = 0x1000;
  uint64_t str_offset = 0xa000;
  Symbols symbols(start_offset, 4 * sizeof(TypeParam), sizeof(TypeParam), str_offset, 0x1000);

  TypeParam sym;
  memset(&sym, 0, sizeof(sym));
  sym.st_shndx = SHN_COMMON;
  sym.st_info = STT_OBJECT | (STB_GLOBAL << 4);
  sym.st_name = 0x100;
  this->memory_.SetMemory(start_offset, &sym, sizeof(sym));
  this->memory_.SetMemory(str_offset + 0x100, "global_0");

  start_offset += sizeof(sym);
  memset(&sym, 0, sizeof(sym));
  sym.st_shndx = SHN_COMMON;
  sym.st_info = STT_FUNC;
  sym.st_name = 0x200;
  sym.st_value = 0x10000;
  sym.st_size = 0x100;
  this->memory_.SetMemory(start_offset, &sym, sizeof(sym));
  this->memory_.SetMemory(str_offset + 0x200, "function_0");

  start_offset += sizeof(sym);
  memset(&sym, 0, sizeof(sym));
  sym.st_shndx = SHN_COMMON;
  sym.st_info = STT_OBJECT | (STB_GLOBAL << 4);
  sym.st_name = 0x300;
  this->memory_.SetMemory(start_offset, &sym, sizeof(sym));
  this->memory_.SetMemory(str_offset + 0x300, "global_1");

  start_offset += sizeof(sym);
  memset(&sym, 0, sizeof(sym));
  sym.st_shndx = SHN_COMMON;
  sym.st_info = STT_FUNC;
  sym.st_name = 0x400;
  sym.st_value = 0x12000;
  sym.st_size = 0x100;
  this->memory_.SetMemory(start_offset, &sym, sizeof(sym));
  this->memory_.SetMemory(str_offset + 0x400, "function_1");

  uint64_t offset;
  EXPECT_TRUE(symbols.GetGlobal<TypeParam>(&this->memory_, "global_0", &offset));
  EXPECT_TRUE(symbols.GetGlobal<TypeParam>(&this->memory_, "global_1", &offset));
  EXPECT_TRUE(symbols.GetGlobal<TypeParam>(&this->memory_, "global_0", &offset));
  EXPECT_TRUE(symbols.GetGlobal<TypeParam>(&this->memory_, "global_1", &offset));

  EXPECT_FALSE(symbols.GetGlobal<TypeParam>(&this->memory_, "function_0", &offset));
  EXPECT_FALSE(symbols.GetGlobal<TypeParam>(&this->memory_, "function_1", &offset));

  std::string name;
  EXPECT_TRUE(symbols.GetName<TypeParam>(0x10002, 0, &this->memory_, &name, &offset));
  EXPECT_EQ("function_0", name);
  EXPECT_EQ(2U, offset);

  EXPECT_TRUE(symbols.GetName<TypeParam>(0x12004, 0, &this->memory_, &name, &offset));
  EXPECT_EQ("function_1", name);
  EXPECT_EQ(4U, offset);
}

REGISTER_TYPED_TEST_CASE_P(SymbolsTest, function_bounds_check, no_symbol, multiple_entries,
                           multiple_entries_nonstandard_size, load_bias, symtab_value_out_of_bounds,
                           symtab_read_cached, get_global);

typedef ::testing::Types<Elf32_Sym, Elf64_Sym> SymbolsTestTypes;
INSTANTIATE_TYPED_TEST_CASE_P(, SymbolsTest, SymbolsTestTypes);

}  // namespace unwindstack
