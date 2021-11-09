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

#ifndef _LIBUNWINDSTACK_ERROR_H
#define _LIBUNWINDSTACK_ERROR_H

#include <stdint.h>

namespace unwindstack {

enum ErrorCode : uint8_t {
  ERROR_NONE,                 // No error.
  ERROR_MEMORY_INVALID,       // Memory read failed.
  ERROR_UNWIND_INFO,          // Unable to use unwind information to unwind.
  ERROR_UNSUPPORTED,          // Encountered unsupported feature.
  ERROR_INVALID_MAP,          // Unwind in an invalid map.
  ERROR_MAX_FRAMES_EXCEEDED,  // The number of frames exceed the total allowed.
  ERROR_REPEATED_FRAME,       // The last frame has the same pc/sp as the next.
  ERROR_INVALID_ELF,          // Unwind in an invalid elf.
};

struct ErrorData {
  ErrorCode code;
  uint64_t address;  // Only valid when code is ERROR_MEMORY_INVALID.
                     // Indicates the failing address.
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_ERROR_H
