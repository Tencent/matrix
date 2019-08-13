/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef _LIBUNWINDSTACK_MACHINE_MIPS_H
#define _LIBUNWINDSTACK_MACHINE_MIPS_H

#include <stdint.h>

namespace unwindstack {

enum MipsReg : uint16_t {
  MIPS_REG_R0 = 0,
  MIPS_REG_R1,
  MIPS_REG_R2,
  MIPS_REG_R3,
  MIPS_REG_R4,
  MIPS_REG_R5,
  MIPS_REG_R6,
  MIPS_REG_R7,
  MIPS_REG_R8,
  MIPS_REG_R9,
  MIPS_REG_R10,
  MIPS_REG_R11,
  MIPS_REG_R12,
  MIPS_REG_R13,
  MIPS_REG_R14,
  MIPS_REG_R15,
  MIPS_REG_R16,
  MIPS_REG_R17,
  MIPS_REG_R18,
  MIPS_REG_R19,
  MIPS_REG_R20,
  MIPS_REG_R21,
  MIPS_REG_R22,
  MIPS_REG_R23,
  MIPS_REG_R24,
  MIPS_REG_R25,
  MIPS_REG_R26,
  MIPS_REG_R27,
  MIPS_REG_R28,
  MIPS_REG_R29,
  MIPS_REG_R30,
  MIPS_REG_R31,
  MIPS_REG_PC,
  MIPS_REG_LAST,

  MIPS_REG_SP = MIPS_REG_R29,
  MIPS_REG_RA = MIPS_REG_R31,
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_MACHINE_MIPS_H