#include "include/buffer_generator.h"

#include <fcntl.h>

namespace test::tools {

    void BufferGenerator::WriteZero(size_t size) {
        for (size_t i = 0; i < size; ++i) {
            buffer_.sputc(0);
        }
    }

    void BufferGenerator::WriteString(const std::string &value) {
        buffer_.sputn(value.c_str(), static_cast<std::streamsize>(value.length()));
    }

    void BufferGenerator::WriteNullTerminatedString(const std::string &value) {
        WriteString(value);
        const size_t length = value.length();
        if (value[length - 1] != '\0') {
            buffer_.sputc('\0');
        }
    }

    std::string BufferGenerator::GetContent() const {
        return std::move(buffer_.str());
    }
}