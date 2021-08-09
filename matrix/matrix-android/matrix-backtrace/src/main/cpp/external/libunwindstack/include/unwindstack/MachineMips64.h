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

#ifndef _LIBUNWINDSTACK_MACHINE_MIPS64_H
#define _LIBUNWINDSTACK_MACHINE_MIPS64_H

#include <stdint.h>

namespace unwindstack {

enum Mips64Reg : uint16_t {
  MIPS64_REG_R0 = 0,
  MIPS64_REG_R1,
  MIPS64_REG_R2,
  MIPS64_REG_R3,
  MIPS64_REG_R4,
  MIPS64_REG_R5,
  MIPS64_REG_R6,
  MIPS64_REG_R7,
  MIPS64_REG_R8,
  MIPS64_REG_R9,
  MIPS64_REG_R10,
  MIPS64_REG_R11,
  MIPS64_REG_R12,
  MIPS64_REG_R13,
  MIPS64_REG_R14,
  MIPS64_REG_R15,
  MIPS64_REG_R16,
  MIPS64_REG_R17,
  MIPS64_REG_R18,
  MIPS64_REG_R19,
  MIPS64_REG_R20,
  MIPS64_REG_R21,
  MIPS64_REG_R22,
  MIPS64_REG_R23,
  MIPS64_REG_R24,
  MIPS64_REG_R25,
  MIPS64_REG_R26,
  MIPS64_REG_R27,
  MIPS64_REG_R28,
  MIPS64_REG_R29,
  MIPS64_REG_R30,
  MIPS64_REG_R31,
  MIPS64_REG_PC,
  MIPS64_REG_LAST,

  MIPS64_REG_SP = MIPS64_REG_R29,
  MIPS64_REG_RA = MIPS64_REG_R31,
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_MACHINE_MIPS64_H