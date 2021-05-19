/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBWECHATBACKTRACE_DEX_FILES_H
#define _LIBWECHATBACKTRACE_DEX_FILES_H

#include <stdint.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <unwindstack/Global.h>
#include <unwindstack/Memory.h>

#include "dex_file_external.h"
#include "DebugGlobal.h"

namespace wechat_backtrace {

    class Maps;

    struct MapInfo;

    class DexFile {
    private:
        ExtDexFile *ext_dex_file_;

    public:
        DexFile(ExtDexFile *ext_dex_file) : ext_dex_file_(ext_dex_file) {}

        ~DexFile() {
            ExtDexFileFree(ext_dex_file_);
        }

        bool
        GetMethodInformation(uint64_t dex_offset, std::string *method_name, uint64_t *method_offset,
                             bool with_signature);

        static std::unique_ptr<DexFile>
        Create(uint64_t dex_file_offset_in_memory, unwindstack::MapInfo *info);
    };

    class DebugDexFiles : public DebugGlobal {
    public:
        explicit DebugDexFiles(std::shared_ptr<unwindstack::Memory> &memory);

        virtual ~DebugDexFiles();

        void
        GetMethodInformation(Maps *maps, unwindstack::MapInfo *info, uint64_t dex_pc,
                             std::string *method_name,
                             uint64_t *method_offset);

        void
        GetMethodInformationImpl(Maps *maps, unwindstack::MapInfo *info,
                                 uint64_t dex_pc, std::string *method_name,
                                 uint64_t *method_offset);

        bool SearchDexFile(uint64_t *addr, unwindstack::MapInfo *info);


        static std::shared_ptr<DebugDexFiles> &Instance();

    private:
        void Init(Maps *maps);

        bool GetAddr(size_t index, uint64_t *addr);

        DexFile *GetDexFile(uint64_t dex_file_offset, unwindstack::MapInfo *info);

        uint64_t ReadEntryPtr32(uint64_t addr);

        uint64_t ReadEntryPtr64(uint64_t addr);

        bool ReadEntry32();

        bool ReadEntry64();

        bool ReadVariableData(uint64_t ptr_offset) override;

        void ProcessArch() override;

        std::mutex lock_;
        bool initialized_ = false;

        uint64_t entry_addr_ = 0;

        uint64_t (DebugDexFiles::*read_entry_ptr_func_)(uint64_t) = nullptr;

        bool (DebugDexFiles::*read_entry_func_)() = nullptr;

        std::unordered_map<uint64_t, std::unique_ptr<DexFile>> files_;

        std::vector<uint64_t> addrs_;
    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_DEX_FILES_H
