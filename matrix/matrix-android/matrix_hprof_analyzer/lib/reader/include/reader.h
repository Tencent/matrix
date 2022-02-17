#ifndef __matrix_hprof_analyzer_reader_h__
#define __matrix_hprof_analyzer_reader_h__

#include <cstdint>
#include <string>

#include "macro.h"

namespace matrix::hprof::internal::reader {

    class Reader {
    public:
        Reader(const uint8_t *source, size_t buffer_size);

        uint8_t ReadU1() {
            return Read(sizeof(uint8_t));
        }

        void SkipU1() {
            Skip(sizeof(uint8_t));
        }

        uint16_t ReadU2() {
            return Read(sizeof(uint16_t));
        }

        void SkipU2() {
            Skip(sizeof(uint16_t));
        }

        uint32_t ReadU4() {
            return Read(sizeof(uint32_t));
        }

        void SkipU4() {
            Skip(sizeof(uint32_t));
        }

        uint64_t ReadU8() {
            return Read(sizeof(uint64_t));
        }

        void SkipU8() {
            Skip(sizeof(uint64_t));
        }

        template<typename T>
        T ReadTyped(size_t size) {
            if (size == sizeof(uint8_t)) {
                const uint8_t value = ReadU1();
                return *reinterpret_cast<const T *>(&value);
            } else if (size == sizeof(uint16_t)) {
                const uint16_t value = ReadU2();
                return *reinterpret_cast<const T *>(&value);
            } else if (size == sizeof(uint32_t)) {
                const uint32_t value = ReadU4();
                return *reinterpret_cast<const T *>(&value);
            } else if (size == sizeof(uint64_t)) {
                const uint64_t value = ReadU8();
                return *reinterpret_cast<const T *>(&value);
            } else {
                throw std::runtime_error("Invalid type size.");
            }
        }

        std::string ReadString(size_t length);

        std::string ReadNullTerminatedString();

        uint64_t Read(size_t size);

        void Skip(size_t size);

        void ResetCursor();

        const void *Extract(size_t size);

    private:
        const size_t buffer_size_;
        const uint8_t *buffer_;
        size_t cursor_;
    };
}

#endif