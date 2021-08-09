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

#include <procinfo/process.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <android-base/unique_fd.h>

using android::base::unique_fd;

namespace android {
namespace procinfo {

bool GetProcessInfo(pid_t tid, ProcessInfo* process_info, std::string* error) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "/proc/%d", tid);

  unique_fd dirfd(open(path, O_DIRECTORY | O_RDONLY));
  if (dirfd == -1) {
    if (error != nullptr) {
      *error = std::string("failed to open ") + path;
    }
    return false;
  }

  return GetProcessInfoFromProcPidFd(dirfd.get(), process_info, error);
}

static ProcessState parse_state(const char* state) {
  switch (*state) {
    case 'R':
      return kProcessStateRunning;
    case 'S':
      return kProcessStateSleeping;
    case 'D':
      return kProcessStateUninterruptibleWait;
    case 'T':
      return kProcessStateStopped;
    case 'Z':
      return kProcessStateZombie;
    default:
      LOG(ERROR) << "unknown process state: " << *state;
      return kProcessStateUnknown;
  }
}

bool GetProcessInfoFromProcPidFd(int fd, ProcessInfo* process_info, std::string* error) {
  int status_fd = openat(fd, "status", O_RDONLY | O_CLOEXEC);

  if (status_fd == -1) {
    if (error != nullptr) {
      *error = "failed to open status fd in GetProcessInfoFromProcPidFd";
    }
    return false;
  }

  std::unique_ptr<FILE, decltype(&fclose)> fp(fdopen(status_fd, "r"), fclose);
  if (!fp) {
    if (error != nullptr) {
      *error = "failed to open status file in GetProcessInfoFromProcPidFd";
    }
    close(status_fd);
    return false;
  }

  int field_bitmap = 0;
  static constexpr int finished_bitmap = 255;
  char* line = nullptr;
  size_t len = 0;

  while (getline(&line, &len, fp.get()) != -1 && field_bitmap != finished_bitmap) {
    char* tab = strchr(line, '\t');
    if (tab == nullptr) {
      continue;
    }

    size_t header_len = tab - line;
    std::string header = std::string(line, header_len);
    if (header == "Name:") {
      std::string name = line + header_len + 1;

      // line includes the trailing newline.
      name.pop_back();
      process_info->name = std::move(name);

      field_bitmap |= 1;
    } else if (header == "Pid:") {
      process_info->tid = atoi(tab + 1);
      field_bitmap |= 2;
    } else if (header == "Tgid:") {
      process_info->pid = atoi(tab + 1);
      field_bitmap |= 4;
    } else if (header == "PPid:") {
      process_info->ppid = atoi(tab + 1);
      field_bitmap |= 8;
    } else if (header == "TracerPid:") {
      process_info->tracer = atoi(tab + 1);
      field_bitmap |= 16;
    } else if (header == "Uid:") {
      process_info->uid = atoi(tab + 1);
      field_bitmap |= 32;
    } else if (header == "Gid:") {
      process_info->gid = atoi(tab + 1);
      field_bitmap |= 64;
    } else if (header == "State:") {
      process_info->state = parse_state(tab + 1);
      field_bitmap |= 128;
    }
  }

  free(line);
  return field_bitmap == finished_bitmap;
}

} /* namespace procinfo */
} /* namespace android */
