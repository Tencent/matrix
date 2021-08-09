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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_LOADER_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_LOADER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace art {

class DexFile;
class DexFileContainer;
class MemMap;
class OatDexFile;

class DexZipArchive;

enum class DexFileLoaderErrorCode {
  kNoError,
  kEntryNotFound,
  kExtractToMemoryError,
  kDexFileError,
  kMakeReadOnlyError,
  kVerifyError
};

// Class that is used to open dex files and deal with corresponding multidex and location logic.
class DexFileLoader {
 public:
  // name of the DexFile entry within a zip archive
  static constexpr const char* kClassesDex = "classes.dex";

  // The separator character in MultiDex locations.
  static constexpr char kMultiDexSeparator = '!';

  // Return true if the magic is valid for dex or cdex.
  static bool IsMagicValid(uint32_t magic);
  static bool IsMagicValid(const uint8_t* magic);

  // Return true if the corresponding version and magic is valid.
  static bool IsVersionAndMagicValid(const uint8_t* magic);

  // Check whether a location denotes a multidex dex file. This is a very simple check: returns
  // whether the string contains the separator character.
  static bool IsMultiDexLocation(const char* location);

  // Return the name of the index-th classes.dex in a multidex zip file. This is classes.dex for
  // index == 0, and classes{index + 1}.dex else.
  static std::string GetMultiDexClassesDexName(size_t index);

  // Return the (possibly synthetic) dex location for a multidex entry. This is dex_location for
  // index == 0, and dex_location + multi-dex-separator + GetMultiDexClassesDexName(index) else.
  static std::string GetMultiDexLocation(size_t index, const char* dex_location);

  // Returns the canonical form of the given dex location.
  //
  // There are different flavors of "dex locations" as follows:
  // the file name of a dex file:
  //     The actual file path that the dex file has on disk.
  // dex_location:
  //     This acts as a key for the class linker to know which dex file to load.
  //     It may correspond to either an old odex file or a particular dex file
  //     inside an oat file. In the first case it will also match the file name
  //     of the dex file. In the second case (oat) it will include the file name
  //     and possibly some multidex annotation to uniquely identify it.
  // canonical_dex_location:
  //     the dex_location where its file name part has been made canonical.
  static std::string GetDexCanonicalLocation(const char* dex_location);

  // For normal dex files, location and base location coincide. If a dex file is part of a multidex
  // archive, the base location is the name of the originating jar/apk, stripped of any internal
  // classes*.dex path.
  static std::string GetBaseLocation(const char* location) {
    const char* pos = strrchr(location, kMultiDexSeparator);
    return (pos == nullptr) ? location : std::string(location, pos - location);
  }

  static std::string GetBaseLocation(const std::string& location) {
    return GetBaseLocation(location.c_str());
  }

  // Returns the '!classes*.dex' part of the dex location. Returns an empty
  // string if there is no multidex suffix for the given location.
  // The kMultiDexSeparator is included in the returned suffix.
  static std::string GetMultiDexSuffix(const std::string& location) {
    size_t pos = location.rfind(kMultiDexSeparator);
    return (pos == std::string::npos) ? std::string() : location.substr(pos);
  }

  virtual ~DexFileLoader() { }

  // Returns the checksums of a file for comparison with GetLocationChecksum().
  // For .dex files, this is the single header checksum.
  // For zip files, this is the zip entry CRC32 checksum for classes.dex and
  // each additional multidex entry classes2.dex, classes3.dex, etc.
  // If a valid zip_fd is provided the file content will be read directly from
  // the descriptor and `filename` will be used as alias for error logging. If
  // zip_fd is -1, the method will try to open the `filename` and read the
  // content from it.
  // Return true if the checksums could be found, false otherwise.
  virtual bool GetMultiDexChecksums(const char* filename,
                                    std::vector<uint32_t>* checksums,
                                    std::string* error_msg,
                                    int zip_fd = -1,
                                    bool* zip_file_only_contains_uncompress_dex = nullptr) const;

  // Opens .dex file, backed by existing memory
  virtual std::unique_ptr<const DexFile> Open(
      const uint8_t* base,
      size_t size,
      const std::string& location,
      uint32_t location_checksum,
      const OatDexFile* oat_dex_file,
      bool verify,
      bool verify_checksum,
      std::string* error_msg,
      std::unique_ptr<DexFileContainer> container = nullptr) const;

  // Open a dex file with a separate data section.
  virtual std::unique_ptr<const DexFile> OpenWithDataSection(
      const uint8_t* base,
      size_t size,
      const uint8_t* data_base,
      size_t data_size,
      const std::string& location,
      uint32_t location_checksum,
      const OatDexFile* oat_dex_file,
      bool verify,
      bool verify_checksum,
      std::string* error_msg) const;


//  // Opens all .dex files found in the memory map, guessing the container format based on file
//  // extension.
//  virtual bool OpenAll(const uint8_t* base,
//                       size_t size,
//                       const std::string& location,
//                       bool verify,
//                       bool verify_checksum,
//                       DexFileLoaderErrorCode* error_code,
//                       std::string* error_msg,
//                       std::vector<std::unique_ptr<const DexFile>>* dex_files) const;

 protected:
  enum class VerifyResult {  // private
    kVerifyNotAttempted,
    kVerifySucceeded,
    kVerifyFailed
  };

  static std::unique_ptr<DexFile> OpenCommon(const uint8_t* base,
                                             size_t size,
                                             const uint8_t* data_base,
                                             size_t data_size,
                                             const std::string& location,
                                             uint32_t location_checksum,
                                             const OatDexFile* oat_dex_file,
                                             bool verify,
                                             bool verify_checksum,
                                             std::string* error_msg,
                                             std::unique_ptr<DexFileContainer> container,
                                             VerifyResult* verify_result);

 private:
  // Open all classesXXX.dex files from a zip archive.
  bool OpenAllDexFilesFromZip(const DexZipArchive& zip_archive,
                              const std::string& location,
                              bool verify,
                              bool verify_checksum,
                              DexFileLoaderErrorCode* error_code,
                              std::string* error_msg,
                              std::vector<std::unique_ptr<const DexFile>>* dex_files) const;

  // Opens .dex file from the entry_name in a zip archive. error_code is undefined when non-null
  // return.
  std::unique_ptr<const DexFile> OpenOneDexFileFromZip(const DexZipArchive& zip_archive,
                                                       const char* entry_name,
                                                       const std::string& location,
                                                       bool verify,
                                                       bool verify_checksum,
                                                       DexFileLoaderErrorCode* error_code,
                                                       std::string* error_msg) const;
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_LOADER_H_
