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

#pragma once

#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <type_traits>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/unique_fd.h>

namespace android {
namespace procinfo {

#if defined(__linux__)

enum ProcessState {
  kProcessStateUnknown,
  kProcessStateRunning,
  kProcessStateSleeping,
  kProcessStateUninterruptibleWait,
  kProcessStateStopped,
  kProcessStateZombie,
};

struct ProcessInfo {
  std::string name;
  ProcessState state;
  pid_t tid;
  pid_t pid;
  pid_t ppid;
  pid_t tracer;
  uid_t uid;
  uid_t gid;
};

// Parse the contents of /proc/<tid>/status into |process_info|.
bool GetProcessInfo(pid_t tid, ProcessInfo* process_info, std::string* error = nullptr);

// Parse the contents of <fd>/status into |process_info|.
// |fd| should be an fd pointing at a /proc/<pid> directory.
bool GetProcessInfoFromProcPidFd(int fd, ProcessInfo* process_info, std::string* error = nullptr);

// Fetch the list of threads from a given process's /proc/<pid> directory.
// |fd| should be an fd pointing at a /proc/<pid> directory.
template <typename Collection>
auto GetProcessTidsFromProcPidFd(int fd, Collection* out, std::string* error = nullptr) ->
    typename std::enable_if<sizeof(typename Collection::value_type) >= sizeof(pid_t), bool>::type {
  out->clear();

  int task_fd = openat(fd, "task", O_DIRECTORY | O_RDONLY | O_CLOEXEC);
  std::unique_ptr<DIR, int (*)(DIR*)> dir(fdopendir(task_fd), closedir);
  if (!dir) {
    if (error != nullptr) {
      *error = "failed to open task directory";
    }
    return false;
  }

  struct dirent* dent;
  while ((dent = readdir(dir.get()))) {
    if (strcmp(dent->d_name, ".") != 0 && strcmp(dent->d_name, "..") != 0) {
      pid_t tid;
      if (!android::base::ParseInt(dent->d_name, &tid, 1, std::numeric_limits<pid_t>::max())) {
        if (error != nullptr) {
          *error = std::string("failed to parse task id: ") + dent->d_name;
        }
        return false;
      }

      out->insert(out->end(), tid);
    }
  }

  return true;
}

template <typename Collection>
auto GetProcessTids(pid_t pid, Collection* out, std::string* error = nullptr) ->
    typename std::enable_if<sizeof(typename Collection::value_type) >= sizeof(pid_t), bool>::type {
  char task_path[PATH_MAX];
  if (snprintf(task_path, PATH_MAX, "/proc/%d", pid) >= PATH_MAX) {
    if (error != nullptr) {
      *error = "task path overflow (pid = " + std::to_string(pid) + ")";
    }
    return false;
  }

  android::base::unique_fd fd(open(task_path, O_DIRECTORY | O_RDONLY | O_CLOEXEC));
  if (fd == -1) {
    if (error != nullptr) {
      *error = std::string("failed to open ") + task_path;
    }
    return false;
  }

  return GetProcessTidsFromProcPidFd(fd.get(), out, error);
}

#endif

} /* namespace procinfo */
} /* namespace android */
