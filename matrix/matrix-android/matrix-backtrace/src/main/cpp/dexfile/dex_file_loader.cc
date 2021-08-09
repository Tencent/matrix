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

#include <android-base/stringprintf.h>
#include "dex_file_loader.h"

//#include "android-base/stringprintf.h"

//#include "base/stl_util.h"
#include "compact_dex_file.h"
#include "dex_file.h"
//#include "dex_file_verifier.h"
#include "standard_dex_file.h"
//#include "ziparchive/zip_archive.h"

namespace art {
//
//namespace {
//
//class VectorContainer : public DexFileContainer {
// public:
//  explicit VectorContainer(std::vector<uint8_t>&& vector) : vector_(std::move(vector)) { }
//  ~VectorContainer() override { }
//
//  int GetPermissions() override {
//    return 0;
//  }
//
//  bool IsReadOnly() override {
//    return true;
//  }
//
//  bool EnableWrite() override {
//    return false;
//  }
//
//  bool DisableWrite() override {
//    return false;
//  }
//
// private:
//  std::vector<uint8_t> vector_;
//  DISALLOW_COPY_AND_ASSIGN(VectorContainer);
//};
//
//}  // namespace
//
//using android::base::StringPrintf;

//class DexZipArchive;
//
//class DexZipEntry {
// public:
//  // Extract this entry to memory.
//  // Returns null on failure and sets error_msg.
//  const std::vector<uint8_t> Extract(std::string* error_msg) {
//    std::vector<uint8_t> map(GetUncompressedLength());
//    if (map.size() == 0) {
//      DCHECK(!error_msg->empty());
//      return map;
//    }
//    const int32_t error = ExtractToMemory(handle_, zip_entry_, map.data(), map.size());
//    if (error) {
//      *error_msg = std::string(ErrorCodeString(error));
//    }
//    return map;
//  }
//
//  virtual ~DexZipEntry() {
//    delete zip_entry_;
//  }
//
//  uint32_t GetUncompressedLength() {
//    return zip_entry_->uncompressed_length;
//  }
//
//  uint32_t GetCrc32() {
//    return zip_entry_->crc32;
//  }
//
// private:
//  DexZipEntry(ZipArchiveHandle handle,
//              ::ZipEntry* zip_entry,
//           const std::string& entry_name)
//    : handle_(handle), zip_entry_(zip_entry), entry_name_(entry_name) {}
//
//  ZipArchiveHandle handle_;
//  ::ZipEntry* const zip_entry_;
//  std::string const entry_name_;
//
//  friend class DexZipArchive;
//  DISALLOW_COPY_AND_ASSIGN(DexZipEntry);
//};
//
//class DexZipArchive {
// public:
//  // return new DexZipArchive instance on success, null on error.
//  static DexZipArchive* Open(const uint8_t* base, size_t size, std::string* error_msg) {
//    ZipArchiveHandle handle;
//    uint8_t* nonconst_base = const_cast<uint8_t*>(base);
//    const int32_t error = OpenArchiveFromMemory(nonconst_base, size, "ZipArchiveMemory", &handle);
//    if (error) {
//      *error_msg = std::string(ErrorCodeString(error));
//      CloseArchive(handle);
//      return nullptr;
//    }
//    return new DexZipArchive(handle);
//  }
//
//  DexZipEntry* Find(const char* name, std::string* error_msg) const {
//    DCHECK(name != nullptr);
//    // Resist the urge to delete the space. <: is a bigraph sequence.
//    std::unique_ptr< ::ZipEntry> zip_entry(new ::ZipEntry);
//    const int32_t error = FindEntry(handle_, name, zip_entry.get());
//    if (error) {
//      *error_msg = std::string(ErrorCodeString(error));
//      return nullptr;
//    }
//    return new DexZipEntry(handle_, zip_entry.release(), name);
//  }
//
//  ~DexZipArchive() {
//    CloseArchive(handle_);
//  }
//
//
// private:
//  explicit DexZipArchive(ZipArchiveHandle handle) : handle_(handle) {}
//  ZipArchiveHandle handle_;
//
//  friend class DexZipEntry;
//  DISALLOW_COPY_AND_ASSIGN(DexZipArchive);
//};
//
//static bool IsZipMagic(uint32_t magic) {
//  return (('P' == ((magic >> 0) & 0xff)) &&
//          ('K' == ((magic >> 8) & 0xff)));
//}

bool DexFileLoader::IsMagicValid(uint32_t magic) {
  return IsMagicValid(reinterpret_cast<uint8_t*>(&magic));
}

bool DexFileLoader::IsMagicValid(const uint8_t* magic) {
  return StandardDexFile::IsMagicValid(magic) ||
      CompactDexFile::IsMagicValid(magic);
}

bool DexFileLoader::IsVersionAndMagicValid(const uint8_t* magic) {
  if (StandardDexFile::IsMagicValid(magic)) {
    return StandardDexFile::IsVersionValid(magic);
  }
  if (CompactDexFile::IsMagicValid(magic)) {
    return CompactDexFile::IsVersionValid(magic);
  }
  return false;
}

bool DexFileLoader::IsMultiDexLocation(const char* location) {
  return strrchr(location, kMultiDexSeparator) != nullptr;
}

//std::string DexFileLoader::GetMultiDexClassesDexName(size_t index) {
//  return (index == 0) ? "classes.dex" : StringPrintf("classes%zu.dex", index + 1);
//}
//
//std::string DexFileLoader::GetMultiDexLocation(size_t index, const char* dex_location) {
//  return (index == 0)
//      ? dex_location
//      : StringPrintf("%s%cclasses%zu.dex", dex_location, kMultiDexSeparator, index + 1);
//}

//std::string DexFileLoader::GetDexCanonicalLocation(const char* dex_location) {
//  CHECK_NE(dex_location, static_cast<const char*>(nullptr));
//  std::string base_location = GetBaseLocation(dex_location);
//  const char* suffix = dex_location + base_location.size();
//  DCHECK(suffix[0] == 0 || suffix[0] == kMultiDexSeparator);
//#ifdef _WIN32
//  // Warning: No symbolic link processing here.
//  PLOG(WARNING) << "realpath is unsupported on Windows.";
//#else
//  // Warning: Bionic implementation of realpath() allocates > 12KB on the stack.
//  // Do not run this code on a small stack, e.g. in signal handler.
//  UniqueCPtr<const char[]> path(realpath(base_location.c_str(), nullptr));
//  if (path != nullptr && path.get() != base_location) {
//    return std::string(path.get()) + suffix;
//  }
//#endif
//  if (suffix[0] == 0) {
//    return base_location;
//  } else {
//    return dex_location;
//  }
//}

// All of the implementations here should be independent of the runtime.
// TODO: implement all the virtual methods.

bool DexFileLoader::GetMultiDexChecksums(
    const char* filename ATTRIBUTE_UNUSED,
    std::vector<uint32_t>* checksums ATTRIBUTE_UNUSED,
    std::string* error_msg,
    int zip_fd ATTRIBUTE_UNUSED,
    bool* zip_file_only_contains_uncompress_dex ATTRIBUTE_UNUSED) const {
  *error_msg = "UNIMPLEMENTED";
  return false;
}

std::unique_ptr<const DexFile> DexFileLoader::Open(
    const uint8_t* base,
    size_t size,
    const std::string& location,
    uint32_t location_checksum,
    const OatDexFile* oat_dex_file,
    bool verify,
    bool verify_checksum,
    std::string* error_msg,
    std::unique_ptr<DexFileContainer> container) const {
  return OpenCommon(base,
                    size,
                    /*data_base=*/ nullptr,
                    /*data_size=*/ 0,
                    location,
                    location_checksum,
                    oat_dex_file,
                    verify,
                    verify_checksum,
                    error_msg,
                    std::move(container),
                    /*verify_result=*/ nullptr);
}

std::unique_ptr<const DexFile> DexFileLoader::OpenWithDataSection(
    const uint8_t* base,
    size_t size,
    const uint8_t* data_base,
    size_t data_size,
    const std::string& location,
    uint32_t location_checksum,
    const OatDexFile* oat_dex_file,
    bool verify,
    bool verify_checksum,
    std::string* error_msg) const {
  return OpenCommon(base,
                    size,
                    data_base,
                    data_size,
                    location,
                    location_checksum,
                    oat_dex_file,
                    verify,
                    verify_checksum,
                    error_msg,
                    /*container=*/ nullptr,
                    /*verify_result=*/ nullptr);
}

//bool DexFileLoader::OpenAll(
//    const uint8_t* base,
//    size_t size,
//    const std::string& location,
//    bool verify,
//    bool verify_checksum,
//    DexFileLoaderErrorCode* error_code,
//    std::string* error_msg,
//    std::vector<std::unique_ptr<const DexFile>>* dex_files) const {
//  DCHECK(dex_files != nullptr) << "DexFile::Open: out-param is nullptr";
//  uint32_t magic = *reinterpret_cast<const uint32_t*>(base);
//  if (IsZipMagic(magic)) {
//    std::unique_ptr<DexZipArchive> zip_archive(DexZipArchive::Open(base, size, error_msg));
//    if (zip_archive.get() == nullptr) {
//      DCHECK(!error_msg->empty());
//      return false;
//    }
//    return OpenAllDexFilesFromZip(*zip_archive.get(),
//                                  location,
//                                  verify,
//                                  verify_checksum,
//                                  error_code,
//                                  error_msg,
//                                  dex_files);
//  }
//  if (IsMagicValid(magic)) {
//    const DexFile::Header* dex_header = reinterpret_cast<const DexFile::Header*>(base);
//    std::unique_ptr<const DexFile> dex_file(Open(base,
//                                                 size,
//                                                 location,
//                                                 dex_header->checksum_,
//                                                 /*oat_dex_file=*/ nullptr,
//                                                 verify,
//                                                 verify_checksum,
//                                                 error_msg));
//    if (dex_file.get() != nullptr) {
//      dex_files->push_back(std::move(dex_file));
//      return true;
//    } else {
//      return false;
//    }
//  }
//  *error_msg = StringPrintf("Expected valid zip or dex file");
//  return false;
//}

std::unique_ptr<DexFile> DexFileLoader::OpenCommon(const uint8_t* base,
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
                                                   VerifyResult* verify_result) {
  if (verify_result != nullptr) {
    *verify_result = VerifyResult::kVerifyNotAttempted;
  }
  std::unique_ptr<DexFile> dex_file;
  if (size >= sizeof(StandardDexFile::Header) && StandardDexFile::IsMagicValid(base)) {
    if (data_size != 0) {
      CHECK_EQ(base, data_base) << "Unsupported for standard dex";
    }
    dex_file.reset(new StandardDexFile(base,
                                       size,
                                       location,
                                       location_checksum,
                                       oat_dex_file,
                                       std::move(container)));
  } else if (size >= sizeof(CompactDexFile::Header) && CompactDexFile::IsMagicValid(base)) {
    if (data_base == nullptr) {
      // TODO: Is there a clean way to support both an explicit data section and reading the one
      // from the header.
      CHECK_EQ(data_size, 0u);
      const CompactDexFile::Header* const header = CompactDexFile::Header::At(base);
      data_base = base + header->data_off_;
      data_size = header->data_size_;
    }
    dex_file.reset(new CompactDexFile(base,
                                      size,
                                      data_base,
                                      data_size,
                                      location,
                                      location_checksum,
                                      oat_dex_file,
                                      std::move(container)));
    // Disable verification for CompactDex input.
    verify = false;
  } else {
    *error_msg = "Invalid or truncated dex file";
  }
  if (dex_file == nullptr) {
//    *error_msg = StringPrintf("Failed to open dex file '%s' from memory: %s", location.c_str(),
//                              error_msg->c_str());
    return nullptr;
  }
  if (!dex_file->Init(error_msg)) {
    dex_file.reset();
    return nullptr;
  }

  (void) verify_checksum;
//  if (verify && !dex::Verify(dex_file.get(),
//                             dex_file->Begin(),
//                             dex_file->Size(),
//                             location.c_str(),
//                             verify_checksum,
//                             error_msg)) {
//    if (verify_result != nullptr) {
//      *verify_result = VerifyResult::kVerifyFailed;
//    }
//    return nullptr;
//  }
//  if (verify_result != nullptr) {
//    *verify_result = VerifyResult::kVerifySucceeded;
//  }
  return dex_file;
}

//std::unique_ptr<const DexFile> DexFileLoader::OpenOneDexFileFromZip(
//    const DexZipArchive& zip_archive,
//    const char* entry_name,
//    const std::string& location,
//    bool verify,
//    bool verify_checksum,
//    DexFileLoaderErrorCode* error_code,
//    std::string* error_msg) const {
//  CHECK(!location.empty());
//  std::unique_ptr<DexZipEntry> zip_entry(zip_archive.Find(entry_name, error_msg));
//  if (zip_entry == nullptr) {
//    *error_code = DexFileLoaderErrorCode::kEntryNotFound;
//    return nullptr;
//  }
//  if (zip_entry->GetUncompressedLength() == 0) {
//    *error_msg = StringPrintf("Dex file '%s' has zero length", location.c_str());
//    *error_code = DexFileLoaderErrorCode::kDexFileError;
//    return nullptr;
//  }
//
//  std::vector<uint8_t> map(zip_entry->Extract(error_msg));
//  if (map.size() == 0) {
//    *error_msg = StringPrintf("Failed to extract '%s' from '%s': %s", entry_name, location.c_str(),
//                              error_msg->c_str());
//    *error_code = DexFileLoaderErrorCode::kExtractToMemoryError;
//    return nullptr;
//  }
//  VerifyResult verify_result;
//  std::unique_ptr<const DexFile> dex_file = OpenCommon(
//      map.data(),
//      map.size(),
//      /*data_base=*/ nullptr,
//      /*data_size=*/ 0u,
//      location,
//      zip_entry->GetCrc32(),
//      /*oat_dex_file=*/ nullptr,
//      verify,
//      verify_checksum,
//      error_msg,
//      std::make_unique<VectorContainer>(std::move(map)),
//      &verify_result);
//  if (verify_result != VerifyResult::kVerifySucceeded) {
//    if (verify_result == VerifyResult::kVerifyNotAttempted) {
//      *error_code = DexFileLoaderErrorCode::kDexFileError;
//    } else {
//      *error_code = DexFileLoaderErrorCode::kVerifyError;
//    }
//    return nullptr;
//  }
//  *error_code = DexFileLoaderErrorCode::kNoError;
//  return dex_file;
//}

// Technically we do not have a limitation with respect to the number of dex files that can be in a
// multidex APK. However, it's bad practice, as each dex file requires its own tables for symbols
// (types, classes, methods, ...) and dex caches. So warn the user that we open a zip with what
// seems an excessive number.
static constexpr size_t kWarnOnManyDexFilesThreshold = 100;

bool DexFileLoader::OpenAllDexFilesFromZip(
    const DexZipArchive& zip_archive,
    const std::string& location,
    bool verify,
    bool verify_checksum,
    DexFileLoaderErrorCode* error_code,
    std::string* error_msg,
    std::vector<std::unique_ptr<const DexFile>>* dex_files) const {
  DCHECK(dex_files != nullptr) << "DexFile::OpenFromZip: out-param is nullptr";
  std::unique_ptr<const DexFile> dex_file(OpenOneDexFileFromZip(zip_archive,
                                                                kClassesDex,
                                                                location,
                                                                verify,
                                                                verify_checksum,
                                                                error_code,
                                                                error_msg));
  if (*error_code != DexFileLoaderErrorCode::kNoError) {
    return false;
  } else {
    // Had at least classes.dex.
    dex_files->push_back(std::move(dex_file));

    // Now try some more.

    // We could try to avoid std::string allocations by working on a char array directly. As we
    // do not expect a lot of iterations, this seems too involved and brittle.

    for (size_t i = 1; ; ++i) {
      std::string name = GetMultiDexClassesDexName(i);
      std::string fake_location = GetMultiDexLocation(i, location.c_str());
      std::unique_ptr<const DexFile> next_dex_file(OpenOneDexFileFromZip(zip_archive,
                                                                         name.c_str(),
                                                                         fake_location,
                                                                         verify,
                                                                         verify_checksum,
                                                                         error_code,
                                                                         error_msg));
      if (next_dex_file.get() == nullptr) {
        if (*error_code != DexFileLoaderErrorCode::kEntryNotFound) {
          LOG(WARNING) << "Zip open failed: " << *error_msg;
        }
        break;
      } else {
        dex_files->push_back(std::move(next_dex_file));
      }

      if (i == kWarnOnManyDexFilesThreshold) {
        LOG(WARNING) << location << " has in excess of " << kWarnOnManyDexFilesThreshold
                     << " dex files. Please consider coalescing and shrinking the number to "
                        " avoid runtime overhead.";
      }

      if (i == std::numeric_limits<size_t>::max()) {
        LOG(ERROR) << "Overflow in number of dex files!";
        break;
      }
    }

    return true;
  }
}
}  // namespace art
