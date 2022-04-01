#include "heap.h"
#include "gtest/gtest.h"

using namespace matrix::hprof::internal::heap;
using namespace matrix::hprof::internal::reader;

TEST(heap_primitive_data, construct) {
    const uint8_t buffer[] = {
            0x01,   // bool: true
            0x00, 0x61,   // char: 'a'
            0x40, 0x40, 0x00, 0x00,   // float: 3.0
            0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // double: 4.0
            0x05,  // byte: 5
            0x00, 0x06,  // short: 6
            0x00, 0x00, 0x00, 0x07,  // int: 7
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,  // long: 8
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // unused
    };
    Reader source_reader(buffer, sizeof(buffer));
    // Boolean.
    {
        const HeapPrimitiveData data(value_type_t::kBoolean, &source_reader);
        EXPECT_EQ(value_type_t::kBoolean, data.GetType());
        EXPECT_EQ(true, data.GetValue<bool>());
    }
    // Char.
    {
        const HeapPrimitiveData data(value_type_t::kChar, &source_reader);
        EXPECT_EQ(value_type_t::kChar, data.GetType());
        EXPECT_EQ('a', data.GetValue<char>());
    }
    // Float.
    {
        const HeapPrimitiveData data(value_type_t::kFloat, &source_reader);
        EXPECT_EQ(value_type_t::kFloat, data.GetType());
        EXPECT_EQ(3.0f, data.GetValue<float>());
    }
    // Double.
    {
        const HeapPrimitiveData data(value_type_t::kDouble, &source_reader);
        EXPECT_EQ(value_type_t::kDouble, data.GetType());
        EXPECT_EQ(4.0, data.GetValue<double>());
    }
    // Byte.
    {
        const HeapPrimitiveData data(value_type_t::kByte, &source_reader);
        EXPECT_EQ(value_type_t::kByte, data.GetType());
        EXPECT_EQ(5, data.GetValue<int8_t>());
    }
    // Short.
    {
        const HeapPrimitiveData data(value_type_t::kShort, &source_reader);
        EXPECT_EQ(value_type_t::kShort, data.GetType());
        EXPECT_EQ(6, data.GetValue<int16_t>());
    }
    // Int.
    {
        const HeapPrimitiveData data(value_type_t::kInt, &source_reader);
        EXPECT_EQ(value_type_t::kInt, data.GetType());
        EXPECT_EQ(7, data.GetValue<int32_t>());
    }
    // Long.
    {
        const HeapPrimitiveData data(value_type_t::kLong, &source_reader);
        EXPECT_EQ(value_type_t::kLong, data.GetType());
        EXPECT_EQ(8, data.GetValue<int64_t>());
    }
    // Not primitive.
    EXPECT_THROW(HeapPrimitiveData(value_type_t::kObject, &source_reader), std::runtime_error);
}

static bool array_equal(const uint8_t *l, const uint8_t *r, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (l[i] != r[i]) {
            return false;
        }
    }
    return true;
}

TEST(heap_primitive_array_data, construct) {
    // Random data.
    const uint8_t buffer[] = {
            0xf1, 0xe7, 0x83, 0xe2, 0xc2, 0x4a, 0x39, 0x2d,
            0x86, 0x49, 0x0e, 0x4e, 0xf2, 0x32, 0x02, 0xef,
            0x73, 0x46, 0x87, 0xda, 0x5e, 0xdf, 0xd0, 0x06,
            0xc3, 0x44, 0x50, 0x08, 0xbf, 0x81, 0x26, 0x5b,
            0xdf, 0x77, 0x0b, 0xc5, 0x6d, 0xe4, 0xc1, 0x7d,
            0xe6, 0x7a, 0x06, 0xf4, 0x4a, 0x28, 0x0f, 0xab,
            0xb4, 0x1f, 0x17, 0x72, 0x9c, 0xc4, 0x53, 0x0e,
            0x2f, 0x71, 0xbe, 0xc4, 0x6f, 0x26, 0x85, 0x91
    };
    // Boolean.
    {
        Reader source_reader(buffer, sizeof(buffer));
        const HeapPrimitiveArrayData data(value_type_t::kBoolean, sizeof(buffer), &source_reader);
        EXPECT_EQ(value_type_t::kBoolean, data.GetType());
        EXPECT_EQ(sizeof(buffer), data.GetSize());
        EXPECT_EQ(64, data.GetLength());
        EXPECT_TRUE(array_equal(buffer, data.GetData(), sizeof(buffer)));
    }
    // Char.
    {
        Reader source_reader(buffer, sizeof(buffer));
        const HeapPrimitiveArrayData data(value_type_t::kChar, sizeof(buffer), &source_reader);
        EXPECT_EQ(value_type_t::kChar, data.GetType());
        EXPECT_EQ(sizeof(buffer), data.GetSize());
        EXPECT_EQ(32, data.GetLength());
        EXPECT_TRUE(array_equal(buffer, data.GetData(), sizeof(buffer)));
    }
    // Float.
    {
        Reader source_reader(buffer, sizeof(buffer));
        const HeapPrimitiveArrayData data(value_type_t::kFloat, sizeof(buffer), &source_reader);
        EXPECT_EQ(value_type_t::kFloat, data.GetType());
        EXPECT_EQ(sizeof(buffer), data.GetSize());
        EXPECT_EQ(16, data.GetLength());
        EXPECT_TRUE(array_equal(buffer, data.GetData(), sizeof(buffer)));
    }
    // Double.
    {
        Reader source_reader(buffer, sizeof(buffer));
        const HeapPrimitiveArrayData data(value_type_t::kDouble, sizeof(buffer), &source_reader);
        EXPECT_EQ(value_type_t::kDouble, data.GetType());
        EXPECT_EQ(sizeof(buffer), data.GetSize());
        EXPECT_EQ(8, data.GetLength());
        EXPECT_TRUE(array_equal(buffer, data.GetData(), sizeof(buffer)));
    }
    // Byte.
    {
        Reader source_reader(buffer, sizeof(buffer));
        const HeapPrimitiveArrayData data(value_type_t::kByte, sizeof(buffer), &source_reader);
        EXPECT_EQ(value_type_t::kByte, data.GetType());
        EXPECT_EQ(sizeof(buffer), data.GetSize());
        EXPECT_EQ(64, data.GetLength());
        EXPECT_TRUE(array_equal(buffer, data.GetData(), sizeof(buffer)));
    }
    // Short.
    {
        Reader source_reader(buffer, sizeof(buffer));
        const HeapPrimitiveArrayData data(value_type_t::kShort, sizeof(buffer), &source_reader);
        EXPECT_EQ(value_type_t::kShort, data.GetType());
        EXPECT_EQ(sizeof(buffer), data.GetSize());
        EXPECT_EQ(32, data.GetLength());
        EXPECT_TRUE(array_equal(buffer, data.GetData(), sizeof(buffer)));
    }
    // Int.
    {
        Reader source_reader(buffer, sizeof(buffer));
        const HeapPrimitiveArrayData data(value_type_t::kInt, sizeof(buffer), &source_reader);
        EXPECT_EQ(value_type_t::kInt, data.GetType());
        EXPECT_EQ(sizeof(buffer), data.GetSize());
        EXPECT_EQ(16, data.GetLength());
        EXPECT_TRUE(array_equal(buffer, data.GetData(), sizeof(buffer)));
    }
    // Long.
    {
        Reader source_reader(buffer, sizeof(buffer));
        const HeapPrimitiveArrayData data(value_type_t::kLong, sizeof(buffer), &source_reader);
        EXPECT_EQ(value_type_t::kLong, data.GetType());
        EXPECT_EQ(sizeof(buffer), data.GetSize());
        EXPECT_EQ(8, data.GetLength());
        EXPECT_TRUE(array_equal(buffer, data.GetData(), sizeof(buffer)));
    }
    // Not primitive.
    {
        Reader source_reader(buffer, sizeof(buffer));
        EXPECT_THROW(HeapPrimitiveArrayData(value_type_t::kObject, sizeof(buffer), &source_reader), std::runtime_error);
    }
}