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

#include <ElfInterfaceArm.h>
#include <ElfWrapper.h>
#include "QuickenMaps.h"
#include <Log.h>
#include <android-base/macros.h>
#include <unwindstack/Symbols.h>
#include <MemoryRange.h>

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    ElfWrapper::~ElfWrapper() {
    }

    bool
    ElfWrapper::Init(QuickenMapInfo *map_info, const std::shared_ptr<Memory> &process_memory,
                     ArchEnum expected_arch) {

        if (elf_vaild) {
            return true;
        }

        soname_ = map_info->name;

        // Elf object should take care of this memory.
        Memory *file_memory = map_info->CreateFileQuickenMemory(process_memory);
        if (UNLIKELY(file_memory == nullptr)) {

            // Return true while it's jit cache region.
            if (IsJitCacheMap(map_info->name)) {
                is_jit_cache_ = true;
                elf_vaild = true;
                QUT_LOG("CreateQuickenMemory name %s, is jit.", map_info->name.c_str());
                return true;
            }

            QUT_LOG("Create quicken interface failed by creating file memory %s failed.",
                    map_info->name.c_str());

            elf_vaild = false;
            return false;
        }

        unique_ptr<Elf> tmp_elf = CreateLightElf(map_info->name, file_memory, expected_arch);
        if (!tmp_elf) {
            // Elf invalid.
            QUT_LOG("Create quicken interface failed by creating elf %s.",
                    map_info->name.c_str());
            elf_vaild = false;
            return false;
        }

        build_id_hex_ = tmp_elf->GetBuildID();
        if (!build_id_hex_.empty()) {
            build_id_ = ToBuildId(build_id_hex_);
        }
        elf_load_bias_ = tmp_elf->GetLoadBias();

        file_backed_elf_ = move(tmp_elf);

        QuickenMemoryFile *tmp_file_memory = dynamic_cast<QuickenMemoryFile *>(file_backed_elf_->memory());
        if (tmp_file_memory) {  // Save file memory
            file_memory_name_ = tmp_file_memory->file_;
            file_memory_offset_ = tmp_file_memory->init_offset_;
            file_memory_size_ = tmp_file_memory->init_size_;
        }

        uint64_t range_offset_end = 0;
        Memory *memory = map_info->CreateQuickenMemory(process_memory, range_offset_end);
        if (UNLIKELY(memory == nullptr)) {
            QUT_LOG("Create quicken interface failed by creating memory %s failed.",
                    map_info->name.c_str());
            elf_vaild = false;
            return false;
        }

        auto light_elf = CreateLightElf(map_info->name, memory, expected_arch);
        if (!light_elf) {
            QUT_LOG("Create quicken interface failed by creating memory backed elf %s.",
                    map_info->name.c_str());
            elf_vaild = false;
            return false;
        }

        memory_ranges_end_ = range_offset_end;

        memory_backed_elf_ = move(light_elf);

        if (file_backed_elf_) {
            memory_backed_elf_->interface()->symbols() = file_backed_elf_->interface()->symbols();
            file_backed_elf_->interface()->symbols().clear();

            memory_backed_elf_->interface()->set_data_offset(
                    file_backed_elf_->interface()->data_offset());
            memory_backed_elf_->interface()->set_data_vaddr_start(
                    file_backed_elf_->interface()->data_vaddr_start());
            memory_backed_elf_->interface()->set_data_vaddr_end(
                    file_backed_elf_->interface()->data_vaddr_end());

            memory_backed_elf_->interface()->set_eh_frame_hdr_offset(
                    file_backed_elf_->interface()->eh_frame_hdr_offset());
            memory_backed_elf_->interface()->set_eh_frame_hdr_section_bias(
                    file_backed_elf_->interface()->eh_frame_hdr_section_bias());
            memory_backed_elf_->interface()->set_eh_frame_hdr_size(
                    file_backed_elf_->interface()->eh_frame_hdr_size());

            memory_backed_elf_->interface()->set_eh_frame_offset(
                    file_backed_elf_->interface()->eh_frame_offset());
            memory_backed_elf_->interface()->set_eh_frame_section_bias(
                    file_backed_elf_->interface()->eh_frame_section_bias());
            memory_backed_elf_->interface()->set_eh_frame_size(
                    file_backed_elf_->interface()->eh_frame_size());

            QUT_LOG("How many symbols: %zu.", memory_backed_elf_->interface()->symbols().size());

            need_symbols_ = ShouldCompleteSymbols(memory_ranges_end_);
            need_debug_data_ = file_backed_elf_->interface()->gnu_debugdata_offset() != 0;
        }

        elf_vaild = true;
        return true;
    }

    bool ElfWrapper::HandOverGnuDebugDataNoLock(unwindstack::Elf *file_backed_elf) {

        if (gnu_debugdata_interface_) {
            return true;
        }

        debug_data_handed_over_ = true;

        memory_backed_elf_->interface()->InitHeaders();

        if (file_backed_elf && file_backed_elf->valid() && file_backed_elf->interface()) {

            QUT_LOG("HandOverGnuDebugData by so %s.", soname_.c_str());

            file_backed_elf->interface()->InitHeaders();
            file_backed_elf->InitGnuDebugdata();
            SetGnuDebugData(file_backed_elf->gnu_debugdata_interface_.get(),
                            file_backed_elf->gnu_debugdata_memory_.get());
            memory_backed_elf_->gnu_debugdata_interface_ = move(
                    file_backed_elf->gnu_debugdata_interface_);
            memory_backed_elf_->gnu_debugdata_memory_ = move(
                    file_backed_elf->gnu_debugdata_memory_);
            return true;
        } else {
            return false;
        }
    }

    bool ElfWrapper::HandOverGnuDebugData() {
        lock_guard<mutex> guard(lock_);
        return HandOverGnuDebugDataNoLock(file_backed_elf_.get());
    }

    bool ElfWrapper::ShouldCompleteSymbols(uint64_t range_offset_end) {

        for (Symbols *symbols : memory_backed_elf_->interface()->symbols()) {
            uint64_t sym_end = symbols->offset() + symbols->count() * symbols->entry_size();
            uint64_t strtab_end = symbols->str_end();
            if (range_offset_end >= sym_end && range_offset_end >= strtab_end) {
                // This should be .dymtab
                continue;
            }

            return true;
        }

        return false;
    }

    bool ElfWrapper::CompleteSymbolsNoLock(uint64_t range_offset_end, uint64_t elf_start_offset) {

        symbols_completed_ = true;

        uint64_t symtab_start = 0;
        uint64_t symtab_end = 0;
        uint64_t linked_strtab_start = 0;
        uint64_t linked_strtab_end = 0;

        for (Symbols *symbols : memory_backed_elf_->interface()->symbols()) {
            uint64_t sym_end = symbols->offset() + symbols->count() * symbols->entry_size();
            uint64_t strtab_end = symbols->str_end();
            if (range_offset_end >= sym_end && range_offset_end >= strtab_end) {
                // This should be .dymtab
                continue;
            }

            symtab_start = symbols->offset();
            symtab_end = sym_end;
            linked_strtab_start = symbols->str_offset();
            linked_strtab_end = strtab_end;
        }

        const uint64_t slim = 2 * 4096;

        MemoryRanges *ranges = dynamic_cast<MemoryRanges *>(memory_backed_elf_->memory());
        uint64_t start = symtab_start;
        uint64_t end = symtab_end;
        for (size_t i = 0; i < 2; i++) {
            if (i == 0) {
                if (linked_strtab_start >= symtab_end && linked_strtab_start - symtab_end <= slim) {
                    // Two sections are close, so merge them.
                    end = linked_strtab_end;
                    i = 2;
                }
            } else {
                start = linked_strtab_start;
                end = linked_strtab_end;
            }

            auto memory = make_shared<QuickenMemoryFile>();
            memory->Init(file_memory_name_, elf_start_offset + start,
                         start - end);
            if (ranges) {
                ranges->Insert(new MemoryRange(memory, 0, memory->Size(), start));
            }
        }

        return true;
    }

    void ElfWrapper::ReleaseFileBackedElf() {
        if (file_backed_elf_) {
            QUT_LOG("ReleaseFileBackedElf for so %s.", soname_.c_str());
            file_backed_elf_.reset(nullptr);
        }
    }

    inline void ElfWrapper::CheckIfSymbolsLoaded() {

        if ((!need_debug_data_ || debug_data_handed_over_) &&
            (!need_symbols_ || symbols_completed_)) {
            return;
        }

        {
            lock_guard<mutex> guard(lock_);
            if (need_symbols_ && !symbols_completed_) {
                QUT_LOG("CompleteSymbolsNoLock for so %s", soname_.c_str());
                CompleteSymbolsNoLock(memory_ranges_end_, file_memory_offset_);
            }
        }

        {
            lock_guard<mutex> guard(lock_);

            if (need_debug_data_ && !debug_data_handed_over_) {

                debug_data_handed_over_ = true; // We do not try this again.

                if (file_memory_name_.empty()) {
                    return;
                }

                // Elf object should take care of this memory.
                std::unique_ptr<QuickenMemoryFile> file_memory = make_unique<QuickenMemoryFile>();
                file_memory->Init(file_memory_name_, file_memory_offset_, file_memory_size_);

                unique_ptr<Elf> tmp_elf = CreateLightElf(soname_, file_memory.release(),
                                                         CURRENT_ARCH);
                if (!tmp_elf) {
                    // Elf invalid.
                    QUT_LOG("Create file backed elf failed by so %s.", soname_.c_str());
                    return;
                }

                HandOverGnuDebugDataNoLock(tmp_elf.get());

                tmp_elf.reset(nullptr);
            }
        }
    }

    BACKTRACE_EXPORT
    bool ElfWrapper::GetFunctionName(uint64_t addr, std::string *name, uint64_t *func_offset) {
        CheckIfSymbolsLoaded();
        if (memory_backed_elf_) {
            return memory_backed_elf_->GetFunctionName(addr, name, func_offset);
        }
        return false;
    }

    void ElfWrapper::FillQuickenInterface(QuickenInterface *quicken_interface_) {

        ArchEnum expected_arch = memory_backed_elf_->arch();

        ElfInterface *elf_interface = memory_backed_elf_->interface();

        if (expected_arch == ARCH_ARM) {
            ElfInterfaceArm *elf_interface_arm = dynamic_cast<ElfInterfaceArm *>(elf_interface);
            quicken_interface_->SetArmExidxInfo(
                    elf_interface_arm->start_offset(),
                    elf_interface_arm->total_entries());
        }

        quicken_interface_->SetEhFrameInfo(
                elf_interface->eh_frame_offset(),
                elf_interface->eh_frame_section_bias(),
                elf_interface->eh_frame_size());
        quicken_interface_->SetEhFrameHdrInfo(
                elf_interface->eh_frame_hdr_offset(),
                elf_interface->eh_frame_hdr_section_bias(),
                elf_interface->eh_frame_hdr_size());

        // Memory backed elf not have debug frame.
        /*
        quicken_interface_->SetDebugFrameInfo(
                elf_interface->debug_frame_offset(),
                elf_interface->debug_frame_section_bias(),
                elf_interface->debug_frame_size());
        */

        if (gnu_debugdata_interface_) {
            quicken_interface_->SetGnuEhFrameInfo(
                    gnu_debugdata_interface_->eh_frame_offset(),
                    gnu_debugdata_interface_->eh_frame_section_bias(),
                    gnu_debugdata_interface_->eh_frame_size());
            quicken_interface_->SetGnuEhFrameHdrInfo(
                    gnu_debugdata_interface_->eh_frame_hdr_offset(),
                    gnu_debugdata_interface_->eh_frame_hdr_section_bias(),
                    gnu_debugdata_interface_->eh_frame_hdr_size());
            quicken_interface_->SetGnuDebugFrameInfo(
                    gnu_debugdata_interface_->debug_frame_offset(),
                    gnu_debugdata_interface_->debug_frame_section_bias(),
                    gnu_debugdata_interface_->debug_frame_size());
        }
    }

    unique_ptr<Elf> ElfWrapper::CreateLightElf(const string &so_path, Memory *memory,
                                               ArchEnum expected_arch) {

        (void) so_path;

        auto elf = make_unique<Elf>(memory);
        elf->Init(true, true); // Ignore .gnu_debug_data
        if (!elf->valid()) {
            QUT_LOG("elf->valid() so %s invalid", so_path.c_str());
            return nullptr;
        }

        if (elf->arch() != expected_arch) {
            QUT_LOG("elf->arch() invalid %s", so_path.c_str());
            return nullptr;
        }

        return elf;
    }


}  // namespace wechat_backtrace

