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

#ifndef ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_EXTERNAL_H_
#define ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_EXTERNAL_H_

// Dex file external API

#include <sys/types.h>
#include <string>
#ifdef __cplusplus
extern "C" {
#endif

// This is the stable C ABI that backs art_api::dex below. Structs and functions
// may only be added here. C++ users should use dex_file_support.h instead.

// Opaque wrapper for an std::string allocated in libdexfile which must be freed
// using ExtDexFileFreeString.
//struct ExtDexFileString;
struct ExtDexFileString {
    const std::string str_;
};

struct ExtDexFileMethodInfo {
  int32_t offset;
  int32_t len;
  const struct ExtDexFileString* name;
};

struct ExtDexFile;

// See art_api::dex::DexFile::OpenFromMemory. Returns true on success.
int ExtDexFileOpenFromMemory(const void* addr,
                             /*inout*/ size_t* size,
                             const char* location,
                             /*out*/ const struct ExtDexFileString** error_msg,
                             /*out*/ struct ExtDexFile** ext_dex_file);

// See art_api::dex::DexFile::GetMethodInfoForOffset. Returns true on success.
int ExtDexFileGetMethodInfoForOffset(struct ExtDexFile* ext_dex_file,
                                     int64_t dex_offset,
                                     int with_signature,
                                     /*out*/ struct ExtDexFileMethodInfo* method_info);

// Frees an ExtDexFile.
void ExtDexFileFree(struct ExtDexFile* ext_dex_file);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_DEX_FILE_EXTERNAL_H_
