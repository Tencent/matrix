#include "reader.h"
#include "gtest/gtest.h"

using namespace matrix::hprof::internal::reader;

TEST(buffer_reader, read_and_skip_big_endian_u1) {
    const uint8_t buffer[] = {0x01, 0x02};
    Reader reader(buffer, sizeof(buffer));
    reader.SkipU1();
    EXPECT_EQ(2, reader.ReadU1());
}

TEST(buffer_reader, read_and_skip_big_endian_u2) {
    const uint8_t buffer[] = {0x00, 0x01, 0x00, 0x02};
    Reader reader(buffer, sizeof(buffer));
    reader.SkipU2();
    EXPECT_EQ(2, reader.ReadU2());
}

TEST(buffer_reader, read_and_skip_big_endian_u4) {
    const uint8_t buffer[] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02};
    Reader reader(buffer, sizeof(buffer));
    reader.SkipU4();
    EXPECT_EQ(2, reader.ReadU4());
}

TEST(buffer_reader, read_and_skip_big_endian_u8) {
    const uint8_t buffer[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
    Reader reader(buffer, sizeof(buffer));
    reader.SkipU8();
    EXPECT_EQ(2, reader.ReadU8());
}

TEST(buffer_reader, read_big_endian_typed) {
    const uint8_t buffer[] = {
            0x01, // bool: true
            0x00, 0x01, // short: 1
            0x3f, 0x80, 0x00, 0x00, // float: 1.0
            0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // double: 2.0
    };
    Reader reader(buffer, sizeof(buffer));
    EXPECT_EQ(true, reader.ReadTyped<bool>(sizeof(uint8_t)));
    EXPECT_EQ(1, reader.ReadTyped<short>(sizeof(uint16_t)));
    EXPECT_EQ(1.0, reader.ReadTyped<float>(sizeof(uint32_t)));
    EXPECT_EQ(2.0, reader.ReadTyped<double>(sizeof(uint64_t)));
    EXPECT_THROW(reader.ReadTyped<long>(sizeof(uint64_t) + sizeof(uint64_t)), std::runtime_error);
}

TEST(buffer_reader, skip_dynamic) {
    const uint8_t buffer[] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    Reader reader(buffer, sizeof(buffer));
    reader.Skip(15);
    EXPECT_EQ(1, reader.ReadU1());
}

TEST(buffer_reader, buffer_overflow) {
    const uint8_t buffer[] = {0x00};
    // Read end of buffer.
    {
        Reader reader(buffer, sizeof(buffer));
        reader.SkipU1();
        EXPECT_THROW(reader.ReadU1(), std::runtime_error);
    }
    // Read out of buffer.
    {
        Reader reader(buffer, sizeof(buffer));
        EXPECT_THROW(reader.ReadU2(), std::runtime_error);
    }
    // Skip end of buffer.
    {
        Reader reader(buffer, sizeof(buffer));
        reader.SkipU1();
        EXPECT_THROW(reader.SkipU1(), std::runtime_error);
    }
    // Skip out of buffer.
    {
        Reader reader(buffer, sizeof(buffer));
        EXPECT_THROW(reader.SkipU2(), std::runtime_error);
    }
}

TEST(buffer_reader, read_string) {
    const std::string buffer = "Hello world!";
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    EXPECT_EQ(buffer, reader.ReadString(buffer.size()));
}

TEST(buffer_reader, read_null_terminated_string) {
    const uint8_t buffer[] = {
            // string: "Hello world!"
            0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f,
            0x72, 0x6c, 0x64, 0x21, 0x00,
            // byte: 1
            0x01
    };
    Reader reader(buffer, sizeof(buffer));
    EXPECT_EQ("Hello world!", reader.ReadNullTerminatedString());
    EXPECT_EQ(1, reader.ReadU1());
}

TEST(buffer_reader, reset_cursor) {
    const uint8_t buffer[] = {0x01, 0x02};
    Reader reader(buffer, sizeof(buffer));
    reader.SkipU1();
    reader.ResetCursor();
    EXPECT_EQ(1, reader.ReadU1());
}