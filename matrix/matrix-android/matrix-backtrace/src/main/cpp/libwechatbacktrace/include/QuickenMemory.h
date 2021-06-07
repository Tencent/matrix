/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBWECHATBACKTRACE_QUICKEN_MEMORY_H
#define _LIBWECHATBACKTRACE_QUICKEN_MEMORY_H

#include <unwindstack/Memory.h>
#include <elf.h>
#include <Log.h>

#define QUICKEN_MEMORY_SLICE EI_NIDENT //getpagesize()

namespace wechat_backtrace {

    class QuickenMemory : public unwindstack::Memory {
    public:
        QuickenMemory() = default;

        virtual ~QuickenMemory() = default;
    };

    class QuickenMemoryFile : public QuickenMemory {
    public:
        QuickenMemoryFile() = default;

        ~QuickenMemoryFile();

        bool Init(const std::string &file, uint64_t offset, uint64_t size = UINT64_MAX);

        size_t Read(uint64_t addr, void *dst, size_t size) override;

        size_t Size() { return size_; }

        void Clear() override;

        std::string file_;
        uint64_t init_offset_;
        uint64_t init_size_;

    protected:

        size_t size_ = 0;
        size_t offset_ = 0;
        uint8_t *data_ = nullptr;

        unsigned char slice_[QUICKEN_MEMORY_SLICE];
        size_t slice_size_ = QUICKEN_MEMORY_SLICE;
    };

    class QuickenMemoryLocal : public unwindstack::Memory {
    public:
        QuickenMemoryLocal() = default;

        virtual ~QuickenMemoryLocal() = default;

        bool IsLocal() const override { return true; }

        size_t Read(uint64_t addr, void *dst, size_t size) override;

        long ReadTag(uint64_t addr) override;
    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_MEMORY_H
