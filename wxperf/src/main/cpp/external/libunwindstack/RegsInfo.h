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

#ifndef _LIBUNWINDSTACK_REGS_INFO_H
#define _LIBUNWINDSTACK_REGS_INFO_H

#include <stdint.h>

#include <unwindstack/Regs.h>

namespace unwindstack {

template <typename AddressType>
struct RegsInfo {
  RegsInfo(RegsImpl<AddressType>* regs) : regs(regs) {}

  RegsImpl<AddressType>* regs = nullptr;
  uint64_t saved_reg_map = 0;
  AddressType saved_regs[64];

  inline AddressType Get(uint32_t reg) {
    if (IsSaved(reg)) {
      return saved_regs[reg];
    }
    return (*regs)[reg];
  }

  inline AddressType* Save(uint32_t reg) {
    if (reg > sizeof(saved_regs) / sizeof(AddressType)) {
      // This should never happen as since all currently supported
      // architectures have the total number of registers < 64.
      abort();
    }
    saved_reg_map |= 1 << reg;
    saved_regs[reg] = (*regs)[reg];
    return &(*regs)[reg];
  }

  inline bool IsSaved(uint32_t reg) {
    if (reg > sizeof(saved_regs) / sizeof(AddressType)) {
      // This should never happen as since all currently supported
      // architectures have the total number of registers < 64.
      abort();
    }
    return saved_reg_map & (1 << reg);
  }

  inline uint16_t Total() { return regs->total_regs(); }
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_REGS_INFO_H
