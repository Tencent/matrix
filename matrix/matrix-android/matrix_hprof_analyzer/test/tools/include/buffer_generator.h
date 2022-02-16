#ifndef __matrix_hprof_analyzer_test_tools_buffer_generator_h__
#define __matrix_hprof_analyzer_test_tools_buffer_generator_h__

#include <sstream>

namespace test::tools {
    class BufferGenerator {
    public:
        template<typename T>
        void Write(T value) {
#pragma clang diagnostic push
#pragma ide diagnostic ignored "Simplify"
#pragma ide diagnostic ignored "UnreachableCode"
            if (BYTE_ORDER == BIG_ENDIAN) {
                buffer_.sputn(reinterpret_cast<const char *>(&value), sizeof(T));
            } else {
                const size_t size = sizeof(T);
                std::unique_ptr<uint8_t[]> temp = std::make_unique<uint8_t[]>(size);
                for (size_t i = 0; i < size; ++i) {
                    temp[i] = reinterpret_cast<uint8_t *>(&value)[size - 1 - i];
                }
                buffer_.sputn(reinterpret_cast<const char *>(temp.get()), static_cast<std::streamsize>(size));
            }
#pragma clang diagnostic pop
        }

        void WriteZero(size_t size);

        void WriteString(const std::string &value);

        void WriteNullTerminatedString(const std::string &value);

        std::string GetContent() const;

    private:
        std::stringbuf buffer_;
    };
}

#endif