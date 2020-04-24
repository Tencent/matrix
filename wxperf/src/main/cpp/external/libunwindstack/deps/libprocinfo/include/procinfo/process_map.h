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

#pragma once

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <functional>
#include <string>
#include <vector>

#include <android-base/file.h>

namespace android {
namespace procinfo {

template <class CallbackType>
bool ReadMapFileContent(char* content, const CallbackType& callback) {
  uint64_t start_addr;
  uint64_t end_addr;
  uint16_t flags;
  uint64_t pgoff;
  ino_t inode;
  char* next_line = content;
  char* p;

  auto pass_space = [&]() {
    if (*p != ' ') {
      return false;
    }
    while (*p == ' ') {
      p++;
    }
    return true;
  };

  auto pass_xdigit = [&]() {
    if (!isxdigit(*p)) {
      return false;
    }
    do {
      p++;
    } while (isxdigit(*p));
    return true;
  };

  while (next_line != nullptr && *next_line != '\0') {
    p = next_line;
    next_line = strchr(next_line, '\n');
    if (next_line != nullptr) {
      *next_line = '\0';
      next_line++;
    }
    // Parse line like: 00400000-00409000 r-xp 00000000 fc:00 426998  /usr/lib/gvfs/gvfsd-http
    char* end;
    // start_addr
    start_addr = strtoull(p, &end, 16);
    if (end == p || *end != '-') {
      return false;
    }
    p = end + 1;
    // end_addr
    end_addr = strtoull(p, &end, 16);
    if (end == p) {
      return false;
    }
    p = end;
    if (!pass_space()) {
      return false;
    }
    // flags
    flags = 0;
    if (*p == 'r') {
      flags |= PROT_READ;
    } else if (*p != '-') {
      return false;
    }
    p++;
    if (*p == 'w') {
      flags |= PROT_WRITE;
    } else if (*p != '-') {
      return false;
    }
    p++;
    if (*p == 'x') {
      flags |= PROT_EXEC;
    } else if (*p != '-') {
      return false;
    }
    p++;
    if (*p != 'p' && *p != 's') {
      return false;
    }
    p++;
    if (!pass_space()) {
      return false;
    }
    // pgoff
    pgoff = strtoull(p, &end, 16);
    if (end == p) {
      return false;
    }
    p = end;
    if (!pass_space()) {
      return false;
    }
    // major:minor
    if (!pass_xdigit() || *p++ != ':' || !pass_xdigit() || !pass_space()) {
      return false;
    }
    // inode
    inode = strtoull(p, &end, 10);
    if (end == p) {
      return false;
    }
    p = end;

    if (*p != '\0' && !pass_space()) {
      return false;
    }

    // filename
    callback(start_addr, end_addr, flags, pgoff, inode, p);
  }
  return true;
}

inline bool ReadMapFile(const std::string& map_file,
                        const std::function<void(uint64_t, uint64_t, uint16_t, uint64_t, ino_t,
                                                 const char*)>& callback) {
  std::string content;
  if (!android::base::ReadFileToString(map_file, &content)) {
    return false;
  }
  return ReadMapFileContent(&content[0], callback);
}

inline bool ReadProcessMaps(pid_t pid,
                            const std::function<void(uint64_t, uint64_t, uint16_t, uint64_t, ino_t,
                                                     const char*)>& callback) {
  return ReadMapFile("/proc/" + std::to_string(pid) + "/maps", callback);
}

struct MapInfo {
  uint64_t start;
  uint64_t end;
  uint16_t flags;
  uint64_t pgoff;
  ino_t inode;
  std::string name;

  MapInfo(uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff, ino_t inode,
          const char* name)
      : start(start), end(end), flags(flags), pgoff(pgoff), inode(inode), name(name) {}
};

inline bool ReadProcessMaps(pid_t pid, std::vector<MapInfo>* maps) {
  return ReadProcessMaps(
      pid, [&](uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff, ino_t inode,
               const char* name) { maps->emplace_back(start, end, flags, pgoff, inode, name); });
}

} /* namespace procinfo */
} /* namespace android */
