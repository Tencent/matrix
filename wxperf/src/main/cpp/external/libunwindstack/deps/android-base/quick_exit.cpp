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

#include "android-base/quick_exit.h"

#if !defined(__linux__)

#include <mutex>
#include <vector>

namespace android {
namespace base {

static auto& quick_exit_mutex = *new std::mutex();
static auto& quick_exit_handlers = *new std::vector<void (*)()>();

void quick_exit(int exit_code) {
  std::lock_guard<std::mutex> lock(quick_exit_mutex);
  for (auto it = quick_exit_handlers.rbegin(); it != quick_exit_handlers.rend(); ++it) {
    (*it)();
  }
  _Exit(exit_code);
}

int at_quick_exit(void (*func)()) {
  std::lock_guard<std::mutex> lock(quick_exit_mutex);
  quick_exit_handlers.push_back(func);
  return 0;
}

}  // namespace base
}  // namespace android
#endif
