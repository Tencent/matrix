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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "DebugDexFiles.h"

#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <Log.h>
#include <Predefined.h>
#include "dex_file_external.h"

namespace wechat_backtrace {

    using namespace unwindstack;

    DEFINE_STATIC_LOCAL(std::shared_ptr<DebugDexFiles>, sInstance_, );
    DEFINE_STATIC_LOCAL(std::mutex, instance_lock_, );

    BACKTRACE_EXPORT
    std::shared_ptr<DebugDexFiles> &DebugDexFiles::Instance() {
        if (sInstance_) {
            return sInstance_;
        }
        std::lock_guard<std::mutex> guard(instance_lock_);
        if (sInstance_) {
            return sInstance_;
        } else {
            auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
            sInstance_.reset(new DebugDexFiles(process_memory));
        }
        return sInstance_;
    }

    bool DexFile::GetMethodInformation(uint64_t dex_offset, std::string *method_name, uint64_t *method_offset, bool with_signature) {

        ExtDexFileMethodInfo ext_method_info;
        if (ExtDexFileGetMethodInfoForOffset(ext_dex_file_,
                                               dex_offset,
                                               with_signature,
                                               &ext_method_info)) {
            if (ext_method_info.offset > 0) {
                *method_name = ext_method_info.name->str_;
                *method_offset = dex_offset - ext_method_info.offset;
                return true;
            }
        }

        return false;
    }


    std::unique_ptr<DexFile> DexFile::Create(uint64_t dex_file_offset_in_memory,
                                             unwindstack::MapInfo *info) {

        size_t max_size = info->end - dex_file_offset_in_memory;
        size_t size = max_size;

        ExtDexFile *ext_dex_file;
        const ExtDexFileString *error_msg;
        int ret = ExtDexFileOpenFromMemory(
                reinterpret_cast<void const *>(dex_file_offset_in_memory),
                /*inout*/ &size,
                info->name.c_str(),
                /*out*/ &error_msg,
                /*out*/ &ext_dex_file);

        if (!ret && error_msg) {
            delete error_msg;
        }

        return std::make_unique<DexFile>(ext_dex_file);
    }

    struct DEXFileEntry32 {
        uint32_t next;
        uint32_t prev;
        uint32_t dex_file;
    };

    struct DEXFileEntry64 {
        uint64_t next;
        uint64_t prev;
        uint64_t dex_file;
    };

    BACKTRACE_EXPORT DebugDexFiles::DebugDexFiles(std::shared_ptr<Memory> &memory) : DebugGlobal(memory) {
        SetArch(unwindstack::Regs::CurrentArch());
        search_libs_.push_back("libart.so");
    }

    BACKTRACE_EXPORT DebugDexFiles::~DebugDexFiles() {}

    void DebugDexFiles::ProcessArch() {
        switch (arch()) {
            case ARCH_ARM:
            case ARCH_MIPS:
            case ARCH_X86:
                read_entry_ptr_func_ = &DebugDexFiles::ReadEntryPtr32;
                read_entry_func_ = &DebugDexFiles::ReadEntry32;
                break;

            case ARCH_ARM64:
            case ARCH_MIPS64:
            case ARCH_X86_64:
                read_entry_ptr_func_ = &DebugDexFiles::ReadEntryPtr64;
                read_entry_func_ = &DebugDexFiles::ReadEntry64;
                break;

            case ARCH_UNKNOWN:
                abort();
        }
    }

    uint64_t DebugDexFiles::ReadEntryPtr32(uint64_t addr) {
        uint32_t entry;
        const uint32_t field_offset = 12;  // offset of first_entry_ in the descriptor struct.
        if (!memory_->ReadFully(addr + field_offset, &entry, sizeof(entry))) {
            return 0;
        }
        return entry;
    }

    uint64_t DebugDexFiles::ReadEntryPtr64(uint64_t addr) {
        uint64_t entry;
        const uint32_t field_offset = 16;  // offset of first_entry_ in the descriptor struct.
        if (!memory_->ReadFully(addr + field_offset, &entry, sizeof(entry))) {
            return 0;
        }
        return entry;
    }

    bool DebugDexFiles::ReadEntry32() {
        DEXFileEntry32 entry;
        if (!memory_->ReadFully(entry_addr_, &entry, sizeof(entry)) || entry.dex_file == 0) {
            entry_addr_ = 0;
            return false;
        }

        addrs_.push_back(entry.dex_file);
        entry_addr_ = entry.next;
        return true;
    }

    bool DebugDexFiles::ReadEntry64() {
        DEXFileEntry64 entry;
        if (!memory_->ReadFully(entry_addr_, &entry, sizeof(entry)) || entry.dex_file == 0) {
            entry_addr_ = 0;
            return false;
        }

        addrs_.push_back(entry.dex_file);
        entry_addr_ = entry.next;
        return true;
    }

    bool DebugDexFiles::ReadVariableData(uint64_t ptr_offset) {
        entry_addr_ = (this->*read_entry_ptr_func_)(ptr_offset);
        return entry_addr_ != 0;
    }

    void DebugDexFiles::Init(Maps *maps) {
        if (initialized_) {
            return;
        }
        initialized_ = true;
        entry_addr_ = 0;

        FindAndReadVariable(maps, "__dex_debug_descriptor");
    }

    bool DebugDexFiles::GetAddr(size_t index, uint64_t *addr) {
        if (index < addrs_.size()) {
            *addr = addrs_[index];
            return true;
        }
        if (entry_addr_ != 0 && (this->*read_entry_func_)()) {
            *addr = addrs_.back();
            return true;
        }
        return false;
    }

    BACKTRACE_EXPORT void
    DebugDexFiles::GetMethodInformation(Maps *maps, unwindstack::MapInfo *info, uint64_t dex_pc,
                                        std::string *method_name, uint64_t *method_offset) {
        if (info->memory_backed_dex_file) {
            DexFile *dex_file = GetDexFile(info->dex_file_offset, info);
            if (dex_file != nullptr &&
                dex_file->GetMethodInformation(dex_pc - info->dex_file_offset, method_name, method_offset,
                                               false)) {
                return;
            }
        }

        GetMethodInformationImpl(maps, info, dex_pc, method_name, method_offset);
    }

    BACKTRACE_EXPORT void
    DebugDexFiles::GetMethodInformationImpl(Maps *maps, unwindstack::MapInfo *info, uint64_t dex_pc,
                                            std::string *method_name, uint64_t *method_offset) {
        std::lock_guard<std::mutex> guard(lock_);   // TODO reduce scope of this lock
        if (!initialized_) {
            Init(maps);
        }

        if (entry_addr_ != 0) {

            size_t index = 0;
            uint64_t addr;
            while (GetAddr(index++, &addr)) {

                if (addr < info->start || addr >= info->end) {
                    continue;
                }

                QUT_LOG("GetMethodInformation dex pc -> %llx, index -> %zu, addr -> %llx, maps -> %s [%llx, %llx](+%llx, +%llx, +%llx)",
                        (uint64_t) dex_pc, index, (uint64_t) addr, info->name.c_str(),
                        (uint64_t) info->start, (uint64_t) info->end, (uint64_t) info->offset,
                        (uint64_t) info->elf_start_offset, (uint64_t) info->elf_offset);

                DexFile *dex_file = GetDexFile(addr, info);
                if (dex_file != nullptr &&
                    dex_file->GetMethodInformation(dex_pc - addr, method_name, method_offset,
                                                   false)) {
                    info->memory_backed_dex_file = true;
                    info->dex_file_offset = addr;
                    break;
                }
            }
        } else {
            uint64_t addr = 0;
            QUT_LOG("GetMethodInformation search dex dex pc -> %llx, maps -> %s [%llx, %llx](+%llx, +%llx, +%llx)",
                    (uint64_t) dex_pc, info->name.c_str(),
                    (uint64_t) info->start, (uint64_t) info->end, (uint64_t) info->offset,
                    (uint64_t) info->elf_start_offset, (uint64_t) info->elf_offset);
            bool ret = SearchDexFile(&addr, info);

            QUT_LOG("GetMethodInformation search dex ret %d, addr -> %llx",
                    ret, (uint64_t) addr);

            if (ret && (addr >= info->start && addr <= info->end)) {
                DexFile *dex_file = GetDexFile(addr, info);
                if (dex_file != nullptr &&
                    dex_file->GetMethodInformation(dex_pc - addr, method_name, method_offset,
                                                   false)) {
                    info->memory_backed_dex_file = true;
                    info->dex_file_offset = addr;
                    return;
                }
            }
        }
    }

    DexFile *DebugDexFiles::GetDexFile(uint64_t dex_file_offset, unwindstack::MapInfo *info) {
        // Lock while processing the data.
        DexFile *dex_file;
        auto entry = files_.find(dex_file_offset);
        if (entry == files_.end()) {
            std::unique_ptr<DexFile> new_dex_file = DexFile::Create(dex_file_offset, info);
            dex_file = new_dex_file.get();
            files_[dex_file_offset] = std::move(new_dex_file);
        } else {
            dex_file = entry->second.get();
        }
        return dex_file;
    }

    bool DebugDexFiles::SearchDexFile(uint64_t *addr, unwindstack::MapInfo *info) {

/*
        Compact dex magic:          {'c', 'd', 'e', 'x' };
        Compact dex magic version:  {'0', '0', '1', '\0'};

        Standard dex magic:         {'d', 'e', 'x', '\n' };
        Standard dex magic version: {'0', '3', '5', '\0'},
                                    // Dex version 036 skipped because of an old dalvik bug on some versions of android where dex
                                    // files with that version number would erroneously be accepted and run.
                                    {'0', '3', '7', '\0'},
                                    // Dex version 038: Android "O" and beyond.
                                    {'0', '3', '8', '\0'},
                                    // Dex version 039: Android "P" and beyond.
                                    {'0', '3', '9', '\0'},
                                    // Dex version 040: beyond Android "10" (previously known as Android "Q").
                                    {'0', '4', '0', '\0'};
*/

        uint64_t _size = info->end - info->start;
        if (_size < 0x1000) {
            return false;
        }

        size_t step_max = _size > 0x1000 ? 0x1000 : (0x1000 - 8);

        char * start_addr = reinterpret_cast<char *>(info->start);
        char * _addr = start_addr;
        bool compact_dex = false;
        bool standard_dex = false;

        // Search 4K range of memory
        for (size_t step = 0; step < step_max;) {
            _addr = start_addr + step;
            if (memcmp(_addr, "dex", 3) == 0) {
                step += 3;
                if ((step > 3) && *(_addr - 1) == 'c' && *(_addr + 3) == '0' && *(_addr + 6) == '\0') {
                    _addr = _addr - 1;
                    compact_dex = true;
                    break;
                }

                if (*(_addr + 3) == '\n' && *(_addr + 4) == '0' && *(_addr + 7) == '\0') {
                    standard_dex = true;
                    break;
                }
            } else {
                step++;
            }
        }


        if (compact_dex || standard_dex) {
            *addr = reinterpret_cast<uint64_t>(_addr);
            return true;
        }

        return false;
    }

}  // namespace unwindstack
