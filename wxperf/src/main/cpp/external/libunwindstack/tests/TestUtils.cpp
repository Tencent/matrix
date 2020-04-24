/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <malloc.h>
#include <stdint.h>

#include <gtest/gtest.h>

namespace unwindstack {

void TestCheckForLeaks(void (*unwind_func)(void*), void* data) {
  static constexpr size_t kNumLeakLoops = 200;
  static constexpr size_t kMaxAllowedLeakBytes = 32 * 1024;

  size_t first_allocated_bytes = 0;
  size_t last_allocated_bytes = 0;
  for (size_t i = 0; i < kNumLeakLoops; i++) {
    unwind_func(data);

    size_t allocated_bytes = mallinfo().uordblks;
    if (first_allocated_bytes == 0) {
      first_allocated_bytes = allocated_bytes;
    } else if (last_allocated_bytes > first_allocated_bytes) {
      // Check that the memory did not increase too much over the first loop.
      ASSERT_LE(last_allocated_bytes - first_allocated_bytes, kMaxAllowedLeakBytes);
    }
    last_allocated_bytes = allocated_bytes;
  }
}

}  // namespace unwindstack
