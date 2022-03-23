#include "errorha.h"
#include "include/reader.h"

#include <sstream>

namespace matrix::hprof::internal::reader {
    Reader::Reader(const uint8_t *source, size_t buffer_size) :
            buffer_size_(buffer_size),
            buffer_(source),
            cursor_(0) {}

    std::string Reader::ReadString(size_t length) {
        const char *content = reinterpret_cast<const char *>(Extract(length));
        return {content, length};
    }

    std::string Reader::ReadNullTerminatedString() {
        std::stringstream stream;
        char current;
        while ((current = static_cast<char>(ReadU1())) != '\0') {
            stream << current;
        }
        return stream.str();
    }

    uint64_t Reader::Read(size_t size) {
        uint64_t ret = 0;
        for (size_t i = 0; i < size; ++i) {
            if (cursor_ >= buffer_size_) {
                pub_fatal("Reach the end of buffer.");
            }
            ret = (ret << 8) | buffer_[cursor_];
            cursor_ += sizeof(uint8_t);
        }
        return ret;
    }

    void Reader::Skip(size_t size) {
        cursor_ += size;
        if (cursor_ > buffer_size_) {
            pub_fatal("Reach the end of buffer.");
        }
    }

    void Reader::ResetCursor() {
        cursor_ = 0;
    }

    const void *Reader::Extract(size_t size) {
        const void *ret = reinterpret_cast<const void *>(buffer_ + cursor_);
        Skip(size);
        return ret;
    }
}