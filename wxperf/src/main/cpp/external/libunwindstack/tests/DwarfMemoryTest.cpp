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

#include <stdint.h>

#include <ios>
#include <vector>

#include <gtest/gtest.h>

#include <unwindstack/DwarfMemory.h>

#include "MemoryFake.h"

namespace unwindstack {

class DwarfMemoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_.Clear();
    dwarf_mem_.reset(new DwarfMemory(&memory_));
  }

  template <typename AddressType>
  void GetEncodedSizeTest(uint8_t value, size_t expected);
  template <typename AddressType>
  void ReadEncodedValue_omit();
  template <typename AddressType>
  void ReadEncodedValue_leb128();
  template <typename AddressType>
  void ReadEncodedValue_data1();
  template <typename AddressType>
  void ReadEncodedValue_data2();
  template <typename AddressType>
  void ReadEncodedValue_data4();
  template <typename AddressType>
  void ReadEncodedValue_data8();
  template <typename AddressType>
  void ReadEncodedValue_non_zero_adjust();
  template <typename AddressType>
  void ReadEncodedValue_overflow();
  template <typename AddressType>
  void ReadEncodedValue_high_bit_set();

  MemoryFake memory_;
  std::unique_ptr<DwarfMemory> dwarf_mem_;
};

TEST_F(DwarfMemoryTest, ReadBytes) {
  memory_.SetMemory(0, std::vector<uint8_t>{0x10, 0x18, 0xff, 0xfe});

  uint8_t byte;
  ASSERT_TRUE(dwarf_mem_->ReadBytes(&byte, 1));
  ASSERT_EQ(0x10U, byte);
  ASSERT_TRUE(dwarf_mem_->ReadBytes(&byte, 1));
  ASSERT_EQ(0x18U, byte);
  ASSERT_TRUE(dwarf_mem_->ReadBytes(&byte, 1));
  ASSERT_EQ(0xffU, byte);
  ASSERT_TRUE(dwarf_mem_->ReadBytes(&byte, 1));
  ASSERT_EQ(0xfeU, byte);
  ASSERT_EQ(4U, dwarf_mem_->cur_offset());

  dwarf_mem_->set_cur_offset(2);
  ASSERT_TRUE(dwarf_mem_->ReadBytes(&byte, 1));
  ASSERT_EQ(0xffU, byte);
  ASSERT_EQ(3U, dwarf_mem_->cur_offset());
}

TEST_F(DwarfMemoryTest, ReadSigned_check) {
  uint64_t value;

  // Signed 8 byte reads.
  memory_.SetData8(0, static_cast<uint8_t>(-10));
  memory_.SetData8(1, 200);
  ASSERT_TRUE(dwarf_mem_->ReadSigned<int8_t>(&value));
  ASSERT_EQ(static_cast<int8_t>(-10), static_cast<int8_t>(value));
  ASSERT_TRUE(dwarf_mem_->ReadSigned<int8_t>(&value));
  ASSERT_EQ(static_cast<int8_t>(200), static_cast<int8_t>(value));

  // Signed 16 byte reads.
  memory_.SetData16(0x10, static_cast<uint16_t>(-1000));
  memory_.SetData16(0x12, 50100);
  dwarf_mem_->set_cur_offset(0x10);
  ASSERT_TRUE(dwarf_mem_->ReadSigned<int16_t>(&value));
  ASSERT_EQ(static_cast<int16_t>(-1000), static_cast<int16_t>(value));
  ASSERT_TRUE(dwarf_mem_->ReadSigned<int16_t>(&value));
  ASSERT_EQ(static_cast<int16_t>(50100), static_cast<int16_t>(value));

  // Signed 32 byte reads.
  memory_.SetData32(0x100, static_cast<uint32_t>(-1000000000));
  memory_.SetData32(0x104, 3000000000);
  dwarf_mem_->set_cur_offset(0x100);
  ASSERT_TRUE(dwarf_mem_->ReadSigned<int32_t>(&value));
  ASSERT_EQ(static_cast<int32_t>(-1000000000), static_cast<int32_t>(value));
  ASSERT_TRUE(dwarf_mem_->ReadSigned<int32_t>(&value));
  ASSERT_EQ(static_cast<int32_t>(3000000000), static_cast<int32_t>(value));

  // Signed 64 byte reads.
  memory_.SetData64(0x200, static_cast<uint64_t>(-2000000000000LL));
  memory_.SetData64(0x208, 5000000000000LL);
  dwarf_mem_->set_cur_offset(0x200);
  ASSERT_TRUE(dwarf_mem_->ReadSigned<int64_t>(&value));
  ASSERT_EQ(static_cast<int64_t>(-2000000000000), static_cast<int64_t>(value));
  ASSERT_TRUE(dwarf_mem_->ReadSigned<int64_t>(&value));
  ASSERT_EQ(static_cast<int64_t>(5000000000000), static_cast<int64_t>(value));
}

TEST_F(DwarfMemoryTest, ReadULEB128) {
  memory_.SetMemory(0, std::vector<uint8_t>{0x01, 0x80, 0x24, 0xff, 0xc3, 0xff, 0x7f});

  uint64_t value;
  ASSERT_TRUE(dwarf_mem_->ReadULEB128(&value));
  ASSERT_EQ(1U, dwarf_mem_->cur_offset());
  ASSERT_EQ(1U, value);

  ASSERT_TRUE(dwarf_mem_->ReadULEB128(&value));
  ASSERT_EQ(3U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0x1200U, value);

  ASSERT_TRUE(dwarf_mem_->ReadULEB128(&value));
  ASSERT_EQ(7U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0xfffe1ffU, value);
}

TEST_F(DwarfMemoryTest, ReadSLEB128) {
  memory_.SetMemory(0, std::vector<uint8_t>{0x06, 0x40, 0x82, 0x34, 0x89, 0x64, 0xf9, 0xc3, 0x8f,
                                            0x2f, 0xbf, 0xc3, 0xf7, 0x5f});

  int64_t value;
  ASSERT_TRUE(dwarf_mem_->ReadSLEB128(&value));
  ASSERT_EQ(1U, dwarf_mem_->cur_offset());
  ASSERT_EQ(6U, value);

  ASSERT_TRUE(dwarf_mem_->ReadSLEB128(&value));
  ASSERT_EQ(2U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0xffffffffffffffc0ULL, static_cast<uint64_t>(value));

  ASSERT_TRUE(dwarf_mem_->ReadSLEB128(&value));
  ASSERT_EQ(4U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0x1a02U, value);

  ASSERT_TRUE(dwarf_mem_->ReadSLEB128(&value));
  ASSERT_EQ(6U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0xfffffffffffff209ULL, static_cast<uint64_t>(value));

  ASSERT_TRUE(dwarf_mem_->ReadSLEB128(&value));
  ASSERT_EQ(10U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0x5e3e1f9U, value);

  ASSERT_TRUE(dwarf_mem_->ReadSLEB128(&value));
  ASSERT_EQ(14U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0xfffffffffbfde1bfULL, static_cast<uint64_t>(value));
}

template <typename AddressType>
void DwarfMemoryTest::GetEncodedSizeTest(uint8_t value, size_t expected) {
  for (size_t i = 0; i < 16; i++) {
    uint8_t encoding = (i << 4) | value;
    ASSERT_EQ(expected, dwarf_mem_->GetEncodedSize<AddressType>(encoding))
        << "encoding 0x" << std::hex << static_cast<uint32_t>(encoding) << " test value 0x"
        << static_cast<size_t>(value);
  }
}

TEST_F(DwarfMemoryTest, GetEncodedSize_absptr_uint32_t) {
  GetEncodedSizeTest<uint32_t>(0, sizeof(uint32_t));
}

TEST_F(DwarfMemoryTest, GetEncodedSize_absptr_uint64_t) {
  GetEncodedSizeTest<uint64_t>(0, sizeof(uint64_t));
}

TEST_F(DwarfMemoryTest, GetEncodedSize_data1) {
  // udata1
  GetEncodedSizeTest<uint32_t>(0x0d, 1);
  GetEncodedSizeTest<uint64_t>(0x0d, 1);

  // sdata1
  GetEncodedSizeTest<uint32_t>(0x0e, 1);
  GetEncodedSizeTest<uint64_t>(0x0e, 1);
}

TEST_F(DwarfMemoryTest, GetEncodedSize_data2) {
  // udata2
  GetEncodedSizeTest<uint32_t>(0x02, 2);
  GetEncodedSizeTest<uint64_t>(0x02, 2);

  // sdata2
  GetEncodedSizeTest<uint32_t>(0x0a, 2);
  GetEncodedSizeTest<uint64_t>(0x0a, 2);
}

TEST_F(DwarfMemoryTest, GetEncodedSize_data4) {
  // udata4
  GetEncodedSizeTest<uint32_t>(0x03, 4);
  GetEncodedSizeTest<uint64_t>(0x03, 4);

  // sdata4
  GetEncodedSizeTest<uint32_t>(0x0b, 4);
  GetEncodedSizeTest<uint64_t>(0x0b, 4);
}

TEST_F(DwarfMemoryTest, GetEncodedSize_data8) {
  // udata8
  GetEncodedSizeTest<uint32_t>(0x04, 8);
  GetEncodedSizeTest<uint64_t>(0x04, 8);

  // sdata8
  GetEncodedSizeTest<uint32_t>(0x0c, 8);
  GetEncodedSizeTest<uint64_t>(0x0c, 8);
}

TEST_F(DwarfMemoryTest, GetEncodedSize_unknown) {
  GetEncodedSizeTest<uint32_t>(0x01, 0);
  GetEncodedSizeTest<uint64_t>(0x01, 0);

  GetEncodedSizeTest<uint32_t>(0x09, 0);
  GetEncodedSizeTest<uint64_t>(0x09, 0);

  GetEncodedSizeTest<uint32_t>(0x0f, 0);
  GetEncodedSizeTest<uint64_t>(0x0f, 0);
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_omit() {
  uint64_t value = 123;
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0xff, &value));
  ASSERT_EQ(0U, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_omit_uint32_t) {
  ReadEncodedValue_omit<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_omit_uint64_t) {
  ReadEncodedValue_omit<uint64_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_absptr_uint32_t) {
  uint64_t value = 100;
  ASSERT_FALSE(dwarf_mem_->ReadEncodedValue<uint32_t>(0x00, &value));

  memory_.SetData32(0, 0x12345678);

  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<uint32_t>(0x00, &value));
  ASSERT_EQ(4U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0x12345678U, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_absptr_uint64_t) {
  uint64_t value = 100;
  ASSERT_FALSE(dwarf_mem_->ReadEncodedValue<uint64_t>(0x00, &value));

  memory_.SetData64(0, 0x12345678f1f2f3f4ULL);

  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<uint64_t>(0x00, &value));
  ASSERT_EQ(8U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0x12345678f1f2f3f4ULL, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_aligned_uint32_t) {
  uint64_t value = 100;
  dwarf_mem_->set_cur_offset(1);
  ASSERT_FALSE(dwarf_mem_->ReadEncodedValue<uint32_t>(0x50, &value));

  memory_.SetData32(4, 0x12345678);

  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<uint32_t>(0x50, &value));
  ASSERT_EQ(8U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0x12345678U, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_aligned_uint64_t) {
  uint64_t value = 100;
  dwarf_mem_->set_cur_offset(1);
  ASSERT_FALSE(dwarf_mem_->ReadEncodedValue<uint64_t>(0x50, &value));

  memory_.SetData64(8, 0x12345678f1f2f3f4ULL);

  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<uint64_t>(0x50, &value));
  ASSERT_EQ(16U, dwarf_mem_->cur_offset());
  ASSERT_EQ(0x12345678f1f2f3f4ULL, value);
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_leb128() {
  memory_.SetMemory(0, std::vector<uint8_t>{0x80, 0x42});

  uint64_t value = 100;
  // uleb128
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x01, &value));
  ASSERT_EQ(0x2100U, value);

  dwarf_mem_->set_cur_offset(0);
  // sleb128
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x09, &value));
  ASSERT_EQ(0xffffffffffffe100ULL, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_leb128_uint32_t) {
  ReadEncodedValue_leb128<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_leb128_uint64_t) {
  ReadEncodedValue_leb128<uint64_t>();
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_data1() {
  memory_.SetData8(0, 0xe0);

  uint64_t value = 0;
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x0d, &value));
  ASSERT_EQ(0xe0U, value);

  dwarf_mem_->set_cur_offset(0);
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x0e, &value));
  ASSERT_EQ(0xffffffffffffffe0ULL, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_data1_uint32_t) {
  ReadEncodedValue_data1<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_data1_uint64_t) {
  ReadEncodedValue_data1<uint64_t>();
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_data2() {
  memory_.SetData16(0, 0xe000);

  uint64_t value = 0;
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x02, &value));
  ASSERT_EQ(0xe000U, value);

  dwarf_mem_->set_cur_offset(0);
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x0a, &value));
  ASSERT_EQ(0xffffffffffffe000ULL, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_data2_uint32_t) {
  ReadEncodedValue_data2<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_data2_uint64_t) {
  ReadEncodedValue_data2<uint64_t>();
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_data4() {
  memory_.SetData32(0, 0xe0000000);

  uint64_t value = 0;
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x03, &value));
  ASSERT_EQ(0xe0000000U, value);

  dwarf_mem_->set_cur_offset(0);
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x0b, &value));
  ASSERT_EQ(0xffffffffe0000000ULL, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_data4_uint32_t) {
  ReadEncodedValue_data4<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_data4_uint64_t) {
  ReadEncodedValue_data4<uint64_t>();
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_data8() {
  memory_.SetData64(0, 0xe000000000000000ULL);

  uint64_t value = 0;
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x04, &value));
  ASSERT_EQ(0xe000000000000000ULL, value);

  dwarf_mem_->set_cur_offset(0);
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x0c, &value));
  ASSERT_EQ(0xe000000000000000ULL, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_data8_uint32_t) {
  ReadEncodedValue_data8<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_data8_uint64_t) {
  ReadEncodedValue_data8<uint64_t>();
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_non_zero_adjust() {
  memory_.SetData64(0, 0xe000000000000000ULL);

  uint64_t value = 0;
  dwarf_mem_->set_pc_offset(0x2000);
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0x14, &value));
  ASSERT_EQ(0xe000000000002000ULL, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_non_zero_adjust_uint32_t) {
  ReadEncodedValue_non_zero_adjust<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_non_zero_adjust_uint64_t) {
  ReadEncodedValue_non_zero_adjust<uint64_t>();
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_overflow() {
  memory_.SetData64(0, 0);

  uint64_t value = 0;
  dwarf_mem_->set_cur_offset(UINT64_MAX);
  ASSERT_FALSE(dwarf_mem_->ReadEncodedValue<AddressType>(0x50, &value));
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_overflow_uint32_t) {
  ReadEncodedValue_overflow<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_overflow_uint64_t) {
  ReadEncodedValue_overflow<uint64_t>();
}

template <typename AddressType>
void DwarfMemoryTest::ReadEncodedValue_high_bit_set() {
  uint64_t value;
  memory_.SetData32(0, 0x15234);
  ASSERT_FALSE(dwarf_mem_->ReadEncodedValue<AddressType>(0xc3, &value));

  dwarf_mem_->set_func_offset(0x60000);
  dwarf_mem_->set_cur_offset(0);
  ASSERT_TRUE(dwarf_mem_->ReadEncodedValue<AddressType>(0xc3, &value));
  ASSERT_EQ(0x75234U, value);
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_high_bit_set_uint32_t) {
  ReadEncodedValue_high_bit_set<uint32_t>();
}

TEST_F(DwarfMemoryTest, ReadEncodedValue_high_bit_set_uint64_t) {
  ReadEncodedValue_high_bit_set<uint64_t>();
}

TEST_F(DwarfMemoryTest, AdjustEncodedValue_absptr) {
  uint64_t value = 0x1234;
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x00, &value));
  ASSERT_EQ(0x1234U, value);
}

TEST_F(DwarfMemoryTest, AdjustEncodedValue_pcrel) {
  uint64_t value = 0x1234;
  ASSERT_FALSE(dwarf_mem_->AdjustEncodedValue(0x10, &value));

  dwarf_mem_->set_pc_offset(0x2000);
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x10, &value));
  ASSERT_EQ(0x3234U, value);

  dwarf_mem_->set_pc_offset(static_cast<uint64_t>(-4));
  value = 0x1234;
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x10, &value));
  ASSERT_EQ(0x1230U, value);
}

TEST_F(DwarfMemoryTest, AdjustEncodedValue_textrel) {
  uint64_t value = 0x8234;
  ASSERT_FALSE(dwarf_mem_->AdjustEncodedValue(0x20, &value));

  dwarf_mem_->set_text_offset(0x1000);
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x20, &value));
  ASSERT_EQ(0x9234U, value);

  dwarf_mem_->set_text_offset(static_cast<uint64_t>(-16));
  value = 0x8234;
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x20, &value));
  ASSERT_EQ(0x8224U, value);
}

TEST_F(DwarfMemoryTest, AdjustEncodedValue_datarel) {
  uint64_t value = 0xb234;
  ASSERT_FALSE(dwarf_mem_->AdjustEncodedValue(0x30, &value));

  dwarf_mem_->set_data_offset(0x1200);
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x30, &value));
  ASSERT_EQ(0xc434U, value);

  dwarf_mem_->set_data_offset(static_cast<uint64_t>(-256));
  value = 0xb234;
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x30, &value));
  ASSERT_EQ(0xb134U, value);
}

TEST_F(DwarfMemoryTest, AdjustEncodedValue_funcrel) {
  uint64_t value = 0x15234;
  ASSERT_FALSE(dwarf_mem_->AdjustEncodedValue(0x40, &value));

  dwarf_mem_->set_func_offset(0x60000);
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x40, &value));
  ASSERT_EQ(0x75234U, value);

  dwarf_mem_->set_func_offset(static_cast<uint64_t>(-4096));
  value = 0x15234;
  ASSERT_TRUE(dwarf_mem_->AdjustEncodedValue(0x40, &value));
  ASSERT_EQ(0x14234U, value);
}

}  // namespace unwindstack
