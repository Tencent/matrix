/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <stdint.h>
#include <sys/mman.h>

#include <memory>
#include <vector>

#include <unwindstack/Elf.h>
#include <unwindstack/Maps.h>
#include <android-base/macros.h>

#include "DebugJit.h"
#include "MemoryRange.h"
#include "BacktraceDefine.h"
#include "QuickenTableGenerator.h"

// This implements the JIT Compilation Interface.
// See https://sourceware.org/gdb/onlinedocs/gdb/JIT-Interface.html

namespace wechat_backtrace {

    using namespace unwindstack;

    DEFINE_STATIC_LOCAL(std::shared_ptr<DebugJit>, sInstance_,);
    DEFINE_STATIC_LOCAL(std::mutex, instance_lock_,);

    BACKTRACE_EXPORT
    std::shared_ptr<DebugJit> &DebugJit::Instance() {
        if (sInstance_) {
            return sInstance_;
        }
        std::lock_guard<std::mutex> guard(instance_lock_);
        if (sInstance_) {
            return sInstance_;
        } else {
            auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
            sInstance_ = std::make_shared<DebugJit>(process_memory);
        }
        return sInstance_;
    }

    struct JITCodeEntry32Pack {
        uint32_t next;
        uint32_t prev;
        uint32_t symfile_addr;
        uint64_t symfile_size;
    } __attribute__((packed));

    struct JITCodeEntry32Pad {
        uint32_t next;
        uint32_t prev;
        uint32_t symfile_addr;
        uint32_t pad;
        uint64_t symfile_size;
    };

    struct JITCodeEntry64 {
        uint64_t next;
        uint64_t prev;
        uint64_t symfile_addr;
        uint64_t symfile_size;
    };

    struct JITDescriptorHeader {
        uint32_t version;
        uint32_t action_flag;
    };

    struct JITDescriptor32 {
        JITDescriptorHeader header;
        uint32_t relevant_entry;
        uint32_t first_entry;
    };

    struct JITDescriptor64 {
        JITDescriptorHeader header;
        uint64_t relevant_entry;
        uint64_t first_entry;
    };

    BACKTRACE_EXPORT
    DebugJit::DebugJit(std::shared_ptr<Memory> &memory) : DebugGlobal(memory) {
        SetArch(unwindstack::Regs::CurrentArch());
        quicken_in_memory_.reset(new QuickenInMemory<addr_t>());
        search_libs_.push_back("libart.so");
    }

    BACKTRACE_EXPORT
    DebugJit::~DebugJit() {
        for (auto *elf : elf_list_) {
            delete elf;
        }
    }

    uint64_t DebugJit::ReadDescriptor32(uint64_t addr) {
        JITDescriptor32 desc;
        if (!memory_->ReadFully(addr, &desc, sizeof(desc))) {
            return 0;
        }

        if (desc.header.version != 1 || desc.first_entry == 0) {
            // Either unknown version, or no jit entries.
            return 0;
        }

        return desc.first_entry;
    }

    uint64_t DebugJit::ReadDescriptor64(uint64_t addr) {
        JITDescriptor64 desc;
        if (!memory_->ReadFully(addr, &desc, sizeof(desc))) {
            return 0;
        }

        if (desc.header.version != 1 || desc.first_entry == 0) {
            // Either unknown version, or no jit entries.
            return 0;
        }

        return desc.first_entry;
    }

    uint64_t DebugJit::ReadEntry32Pack(uint64_t *start, uint64_t *size) {
        JITCodeEntry32Pack code;
        if (!memory_->ReadFully(entry_addr_, &code, sizeof(code))) {
            return 0;
        }

        *start = code.symfile_addr;
        *size = code.symfile_size;
        return code.next;
    }

    uint64_t DebugJit::ReadEntry32Pad(uint64_t *start, uint64_t *size) {
        JITCodeEntry32Pad code;
        if (!memory_->ReadFully(entry_addr_, &code, sizeof(code))) {
            return 0;
        }

        *start = code.symfile_addr;
        *size = code.symfile_size;
        return code.next;
    }

    uint64_t DebugJit::ReadEntry64(uint64_t *start, uint64_t *size) {
        JITCodeEntry64 code;
        if (!memory_->ReadFully(entry_addr_, &code, sizeof(code))) {
            return 0;
        }

        *start = code.symfile_addr;
        *size = code.symfile_size;
        return code.next;
    }

    void DebugJit::ProcessArch() {
        switch (arch()) {
            case ARCH_X86:
                read_descriptor_func_ = &DebugJit::ReadDescriptor32;
                read_entry_func_ = &DebugJit::ReadEntry32Pack;
                break;

            case ARCH_ARM:
            case ARCH_MIPS:
                read_descriptor_func_ = &DebugJit::ReadDescriptor32;
                read_entry_func_ = &DebugJit::ReadEntry32Pad;
                break;

            case ARCH_ARM64:
            case ARCH_X86_64:
            case ARCH_MIPS64:
                read_descriptor_func_ = &DebugJit::ReadDescriptor64;
                read_entry_func_ = &DebugJit::ReadEntry64;
                break;
            case ARCH_UNKNOWN:
                abort();
        }
    }

    bool DebugJit::ReadVariableData(uint64_t ptr) {
        entry_addr_ = (this->*read_descriptor_func_)(ptr);
        return entry_addr_ != 0;
    }

    void DebugJit::Init(Maps *maps) {
        if (initialized_) {
            return;
        }
        // Regardless of what happens below, consider the init finished.
        initialized_ = true;

        FindAndReadVariable(maps, "__jit_debug_descriptor");
    }

    BACKTRACE_EXPORT
    unwindstack::Elf *DebugJit::GetElf(Maps *maps, uint64_t pc) {
        // Use a single lock, this object should be used so infrequently that
        // a fine grain lock is unnecessary.
        std::lock_guard<std::mutex> guard(lock_);
        if (!initialized_) {
            Init(maps);
        }

        // Search the existing elf object first.
        for (Elf *elf : elf_list_) {
            if (elf->IsValidPc(pc)) {
                return elf;
            }
        }

        while (entry_addr_ != 0) {
            uint64_t start;
            uint64_t size;
            entry_addr_ = (this->*read_entry_func_)(&start, &size);

            unwindstack::Elf *elf = new unwindstack::Elf(new MemoryRange(memory_, start, size, 0));
            elf->Init();
            if (!elf->valid()) {
                // The data is not formatted in a way we understand, do not attempt
                // to process any other entries.
                entry_addr_ = 0;
                delete elf;
                return nullptr;
            }
            elf_list_.push_back(elf);

            if (elf->IsValidPc(pc)) {
                return elf;
            }
        }

        QUT_LOG("DebugJit entry_addr_ %llx", entry_addr_);
        return nullptr;
    }

    bool DebugJit::GetFutSectionsInMemory(
            Maps *maps,
            uint64_t pc,
            /* out */ std::shared_ptr<wechat_backtrace::QutSectionsInMemory> &fut_sections) {

        if (UNLIKELY(!quicken_in_memory_)) {
            return false;
        }

        if (quicken_in_memory_->FindInCache(pc, fut_sections)) {
            return true;
        }

        unwindstack::Elf *elf = GetElf(maps, pc);

        if (!elf) {
            return false;
        }

        return quicken_in_memory_->GenerateFutSectionsInMemoryForJIT(
                elf, memory_.get(), pc, fut_sections);

    }


}  // namespace unwindstack
