#include <stdint.h>
#include <bits/pthread_types.h>
#include <cstdlib>
#include <pthread.h>
#include <android-base/strings.h>
#include <unwindstack/DwarfError.h>
#include <dlfcn.h>
#include <cinttypes>
#include <sys/mman.h>
#include <fcntl.h>
#include <android-base/unique_fd.h>
#include <sys/stat.h>
#include <Log.h>
#include <stdio.h>

#include <unwindstack/Elf.h>
#include <BacktraceDefine.h>
#include "QuickenMemory.h"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    QuickenMemoryFile::~QuickenMemoryFile() {
        Clear();
    }

    void QuickenMemoryFile::Clear() {
        if (data_) {
            munmap(&data_[-offset_], size_ + offset_);
            data_ = nullptr;
        }
    }

    inline static size_t
    memory_copy(void *dst, uint8_t *data, size_t copy_size, size_t offset, size_t data_size) {
        if (offset >= data_size) {
            return 0;
        }

        size_t bytes_left = data_size - static_cast<size_t>(offset);
        const unsigned char *actual_base = static_cast<const unsigned char *>(data) + offset;
        size_t actual_len = std::min(bytes_left, copy_size);

        memcpy(dst, actual_base, actual_len);

        return actual_len;
    }

    bool QuickenMemoryFile::Init(const std::string &file, uint64_t offset, uint64_t size) {

        // Clear out any previous data if it exists.
        Clear();

        if (offset == 0) {
            slice_size_ = QUICKEN_MEMORY_SLICE;
        } else {
            slice_size_ = 0;
        }

        android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(file.c_str(), O_RDONLY | O_CLOEXEC)));
        if (fd == -1) {
            return false;
        }

        struct stat buf;
        if (fstat(fd, &buf) == -1) {
            return false;
        }
        if (offset >= static_cast<uint64_t>(buf.st_size)) {
            return false;
        }

        offset_ = offset & (getpagesize() - 1);
        uint64_t aligned_offset = offset & ~(getpagesize() - 1);
        if (aligned_offset > static_cast<uint64_t>(buf.st_size) ||
            offset > static_cast<uint64_t>(buf.st_size)) {
            return false;
        }

        size_ = buf.st_size - aligned_offset;
        uint64_t max_size;
        if (!__builtin_add_overflow(size, offset_, &max_size) && max_size < size_) {
            // Truncate the mapped size.
            size_ = max_size;
        }

        void *map = mmap(nullptr, size_, PROT_WRITE | PROT_READ, MAP_PRIVATE, fd, aligned_offset);
        if (map == MAP_FAILED) {
            return false;
        }

        if (slice_size_ > 0) {
            // Cut off e_ident from elf mapping to broke elf header's completeness. So we can prevent
            // some kind of custom loaders finding our mapping elf as the one loaded by dl.
            size_t slice_size = slice_size_;
            memcpy(slice_, map, slice_size);
            memset(map, 0, slice_size);
        }

        mprotect(map, size_, PROT_READ);

        data_ = &reinterpret_cast<uint8_t *>(map)[offset_];
        size_ -= offset_;

        return true;
    }

    size_t QuickenMemoryFile::Read(uint64_t addr, void *dst, size_t size) {
        size_t actual_len;
        if (addr < slice_size_) {
            actual_len = memory_copy(dst, slice_, size, addr, slice_size_);
            if (addr + size > slice_size_) {
                size -= (slice_size_ - addr);
                addr = slice_size_;
                dst = &(reinterpret_cast<uint8_t *>(dst)[actual_len]);
                actual_len += memory_copy(dst, data_, size, addr, size_);
            }
        } else {
            actual_len = memory_copy(dst, data_, size, addr, size_);
        }

        return actual_len;
    }

    // ------------------------------------------------------------------------------------

    size_t QuickenMemoryLocal::Read(uint64_t remote_src, void* dst, size_t len) {
        memcpy(dst, reinterpret_cast<const void *>(remote_src), len);   // Should we use process_vm_readv()?
        return len;
    }

    long QuickenMemoryLocal::ReadTag(uint64_t) {
        return -1;
    }

} // namespace wechat_backtrace