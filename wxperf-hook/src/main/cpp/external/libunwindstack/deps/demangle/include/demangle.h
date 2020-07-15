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

#ifndef __LIB_DEMANGLE_H_
#define __LIB_DEMANGLE_H_

#include <string>

// If the name cannot be demangled, the original name will be returned as
// a std::string. If the name can be demangled, then the demangled name
// will be returned as a std::string.
std::string demangle(const char* name);

#endif  // __LIB_DEMANGLE_H_
