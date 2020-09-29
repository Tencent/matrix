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

#ifndef _LIBUNWINDSTACK_FAST_ARM_EXIDX_H
#define _LIBUNWINDSTACK_FAST_ARM_EXIDX_H

#include <stdint.h>

#include <deque>
#include <map>
#include "ArmExidx.h"

#include "FastRegs.h"

namespace unwindstack {

// Forward declarations.
class Memory;
class RegsArm;

class FastArmExidx {
public:
    FastArmExidx(uintptr_t *regs, Memory *elf_memory, Memory *process_memory)
            : regs_(regs), elf_memory_(elf_memory), process_memory_(process_memory) {}

    virtual ~FastArmExidx() {}

    bool ExtractEntryData(uint32_t entry_offset);

    bool Eval();

    bool Decode();


    ArmStatus status() { return status_; }

    uint64_t status_address() { return status_address_; }

    uint32_t cfa() { return cfa_; }

    void set_cfa(uint32_t cfa) { cfa_ = cfa; }

    bool pc_set() { return pc_set_; }

    void set_pc_set(bool pc_set) { pc_set_ = pc_set; }

private:
    bool GetByte(uint8_t *byte);

    void AdjustRegisters(int32_t offset);

    bool DecodePrefix_10_00(uint8_t byte);

    bool DecodePrefix_10_01(uint8_t byte);

    bool DecodePrefix_10_10(uint8_t byte);

    bool DecodePrefix_10_11_0000();

    bool DecodePrefix_10_11_0001();

    bool DecodePrefix_10_11_0010();

    bool DecodePrefix_10_11_0011();

    bool DecodePrefix_10_11_01nn();

    bool DecodePrefix_10_11_1nnn(uint8_t byte);

    bool DecodePrefix_10(uint8_t byte);

    bool DecodePrefix_11_000(uint8_t byte);

    bool DecodePrefix_11_001(uint8_t byte);

    bool DecodePrefix_11_010(uint8_t byte);

    bool DecodePrefix_11(uint8_t byte);

    uintptr_t *regs_ = nullptr;
    uint32_t cfa_ = 0;

    uint8_t *data_array = nullptr;
    size_t data_index = 0;
    size_t data_read_index = 0;

    ArmStatus status_ = ARM_STATUS_NONE;
    uint64_t status_address_ = 0;

    Memory *elf_memory_;
    Memory *process_memory_;

    bool pc_set_ = false;
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_FAST_ARM_EXIDX_H
