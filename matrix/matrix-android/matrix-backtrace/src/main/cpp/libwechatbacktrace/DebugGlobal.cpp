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
#include <string.h>
#include <sys/mman.h>
#include <libgen.h>

#include <string>
#include <vector>

#include <unwindstack/Memory.h>
#include <QuickenMaps.h>

#include "DebugGlobal.h"

namespace wechat_backtrace {

    using namespace unwindstack;

    DebugGlobal::DebugGlobal(std::shared_ptr<Memory> &memory) : memory_(memory) {}

    DebugGlobal::DebugGlobal(std::shared_ptr<Memory> &memory, std::vector<std::string> &search_libs)
            : memory_(memory), search_libs_(search_libs) {}

    BACKTRACE_EXPORT
    void DebugGlobal::SetArch(ArchEnum arch) {
        if (arch_ == ARCH_UNKNOWN) {
            arch_ = arch;
            ProcessArch();
        }
    }

    bool DebugGlobal::Searchable(const std::string &name) {
        if (search_libs_.empty()) {
            return true;
        }

        if (name.empty()) {
            return false;
        }

        const char *base_name = basename(name.c_str());
        for (const std::string &lib : search_libs_) {
            if (base_name == lib) {
                return true;
            }
        }
        return false;
    }

    void DebugGlobal::FindAndReadVariable(wechat_backtrace::Maps *maps, const char *var_str) {
        std::string variable(var_str);
        // When looking for DebugGlobal variables, do not arbitrarily search every
        // readable map. Instead look for a specific pattern that must exist.
        // The pattern should be a readable map, followed by a read-write
        // map with a non-zero offset.
        // For example:
        //   f0000-f1000 0 r-- /system/lib/libc.so
        //   f1000-f2000 1000 r-x /system/lib/libc.so
        //   f2000-f3000 2000 rw- /system/lib/libc.so
        // This also works:
        //   f0000-f2000 0 r-- /system/lib/libc.so
        //   f2000-f3000 2000 rw- /system/lib/libc.so
        // It is also possible to see empty maps after the read-only like so:
        //   f0000-f1000 0 r-- /system/lib/libc.so
        //   f1000-f2000 0 ---
        //   f2000-f3000 1000 r-x /system/lib/libc.so
        //   f3000-f4000 2000 rw- /system/lib/libc.so
        wechat_backtrace::QuickenMapInfo *map_zero = nullptr;
        wechat_backtrace::QuickenMapInfo *map_rx = nullptr;
        bool find_rw_map = false;
        uint64_t candidate_ptr;

        for (size_t i = 0; i < maps->maps_size_; i++) {
            auto info = maps->local_maps_[i];
            if (info->offset != 0) {
                if (find_rw_map &&
                    (info->flags & (PROT_READ | PROT_WRITE)) == (PROT_READ | PROT_WRITE) &&
                    map_rx != nullptr && info->name == map_rx->name
                        ) {
                    uint64_t offset_end = info->offset + info->end - info->start;
                    if (candidate_ptr >= info->offset && candidate_ptr < offset_end) {
                        candidate_ptr = info->start + candidate_ptr - info->offset;
                        if (ReadVariableData(candidate_ptr)) {
                            break;  // Found
                        }
                    }
                }
                if ((info->flags & (PROT_READ | PROT_EXEC)) == (PROT_READ | PROT_EXEC) &&
                    map_zero != nullptr && Searchable(info->name) && info->name == map_zero->name) {
                    QuickenInterface *interface = info->GetQuickenInterface(memory_,
                                                                            CURRENT_ARCH);
                    if (!interface || !interface->elf_wrapper_) {
                        QUT_LOG("FindAndReadVariable failed for so %s, elf invalid", info->name.c_str());
                        continue;
                    }
                    Elf *elf = interface->elf_wrapper_->GetMemoryBackedElf().get();
                    uint64_t ptr;
                    if (elf->GetGlobalVariableOffset(variable, &ptr) && ptr != 0) {
                        find_rw_map = true;
                        map_rx = info;
                        candidate_ptr = ptr;
                    } else
                        QUT_LOG("FindAndReadVariable get global variable offset failed fo so %s", info->name.c_str());
                }
            } else if (info->offset == 0 && !info->name.empty()) {
                map_zero = info;
            }
        }
    }

}  // namespace unwindstack
