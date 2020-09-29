/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef _LIBUNWINDSTACK_FAST_ARM_EXIDX_INTERFACE_H
#define _LIBUNWINDSTACK_FAST_ARM_EXIDX_INTERFACE_H

#include <elf.h>
#include <stdint.h>

#include <iterator>
#include <unordered_map>

#include <unwindstack/ElfInterface.h>
#include <unwindstack/Memory.h>
#include <memory>
#include "QuickenTableGenerator.h"

namespace unwindstack {

class FastArmExidxInterface {

public:
    FastArmExidxInterface(Memory* memory, uint64_t load_bias, uint64_t start_offset
            , uint64_t total_entries) : memory_(memory), load_bias_(load_bias)
            , start_offset_(start_offset), total_entries_(total_entries) {
        addrs_array_ = new uint32_t[total_entries_]();
        addrs_array_ptr_.reset(addrs_array_);
    }

    virtual ~FastArmExidxInterface() = default;

    bool GetPrel31Addr(uint32_t offset, uint32_t* addr);

    bool FindEntry(uint32_t pc, uint64_t* entry_offset);

    bool Step(uint64_t pc, uint32_t* regs, Memory* process_memory, bool* finished);

protected:
    std::mutex lock_;

    unwindstack::Memory* memory_;
    uint64_t load_bias_ = 0;
    uint64_t start_offset_;
    uint64_t total_entries_;

    ErrorData last_error_;

    std::unique_ptr<uint32_t> addrs_array_ptr_;
    uint32_t* addrs_array_;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_FAST_ARM_EXIDX_INTERFACE_H
