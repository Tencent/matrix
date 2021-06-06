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

#ifndef _LIBWECHATBACKTRACE_ELF_WRAPPER_H
#define _LIBWECHATBACKTRACE_ELF_WRAPPER_H

#include "BacktraceDefine.h"

namespace wechat_backtrace {

    class QuickenMapInfo;
    class QuickenInterface;

    class ElfWrapper {
    public:
        ElfWrapper() {};

        ~ElfWrapper();

        static std::unique_ptr<unwindstack::Elf>
        CreateLightElf(
                const std::string &so_path,
                unwindstack::Memory *memory,
                unwindstack::ArchEnum expected_arch);

        bool Init(
                QuickenMapInfo *map_info,
                const std::shared_ptr<unwindstack::Memory> &process_memory,
                unwindstack::ArchEnum expected_arch);

        void ReleaseFileBackedElf();

        bool HandOverGnuDebugData();

        bool ShouldCompleteSymbols(uint64_t range_offset_end);

        void FillQuickenInterface(QuickenInterface *quicken_interface_);

        bool GetFunctionName(uint64_t addr, std::string* name, uint64_t* func_offset);

        void CheckIfSymbolsLoaded();

        unwindstack::ElfInterface *GetGnuDebugDataInterface() {
            return gnu_debugdata_interface_;
        }

        const std::unique_ptr<unwindstack::Elf> &GetMemoryBackedElf() const {
            return memory_backed_elf_;
        }

        const std::unique_ptr<unwindstack::Elf> &GetFileBackedElf() const {
            return file_backed_elf_;
        }

        const std::string &GetSoname() const {
            return soname_;
        }

        const std::string &GetBuildIdHex() const {
            return build_id_hex_;
        }

        const std::string &GetBuildId() const {
            return build_id_;
        }

        uint64_t GetElfLoadBias() const {
            return elf_load_bias_;
        }

        bool IsJitCache() const {
            return is_jit_cache_;
        }
    private:

        void SetGnuDebugData(unwindstack::ElfInterface *interface, unwindstack::Memory* memory) {
            gnu_debugdata_interface_ = interface;
            gnu_debugdata_memory_ = memory;
        }

        bool HandOverGnuDebugDataNoLock(unwindstack::Elf *file_backed_elf);

        bool CompleteSymbolsNoLock(uint64_t range_offset_end, uint64_t elf_start_offset);

        std::unique_ptr<unwindstack::Elf> memory_backed_elf_;
        std::unique_ptr<unwindstack::Elf> file_backed_elf_;

        std::string file_memory_name_;
        uint64_t file_memory_offset_;
        uint64_t file_memory_size_;

        std::string soname_;

        std::string build_id_hex_;
        std::string build_id_;

        uint64_t elf_load_bias_ = 0;

        bool is_jit_cache_ = false;

        bool elf_vaild = false;

        std::mutex lock_;

        uint64_t memory_ranges_end_ = 0;
        volatile bool need_symbols_ = false;
        volatile bool need_debug_data_ = false;
        volatile bool symbols_completed_ = false;
        volatile bool debug_data_handed_over_ = false;
        unwindstack::ElfInterface* gnu_debugdata_interface_ = nullptr;
        unwindstack::Memory* gnu_debugdata_memory_ = nullptr;

    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_ELF_WRAPPER_H
