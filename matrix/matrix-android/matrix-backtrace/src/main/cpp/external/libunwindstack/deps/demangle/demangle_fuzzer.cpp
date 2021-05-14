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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "Demangler.h"

extern "C" void LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<char> data_str(size + 1);
  memcpy(data_str.data(), data, size);
  data_str[size] = '\0';

  Demangler demangler;
  std::string demangled_name = demangler.Parse(data_str.data());
  if (size != 0 && data_str[0] != '\0' && demangled_name.empty()) {
    abort();
  }
}
