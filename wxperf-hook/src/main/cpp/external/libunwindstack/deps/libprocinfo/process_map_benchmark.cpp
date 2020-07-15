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

#include <procinfo/process_map.h>

#include <string.h>
#include <sys/types.h>

#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <backtrace/BacktraceMap.h>
#include <unwindstack/Maps.h>

#include <benchmark/benchmark.h>

static void BM_ReadMapFile(benchmark::State& state) {
  std::string map_file = android::base::GetExecutableDirectory() + "/testdata/maps";
  for (auto _ : state) {
    std::vector<android::procinfo::MapInfo> maps;
    android::procinfo::ReadMapFile(map_file, [&](uint64_t start, uint64_t end, uint16_t flags,
                                                 uint64_t pgoff, ino_t inode, const char* name) {
      maps.emplace_back(start, end, flags, pgoff, inode, name);
    });
    CHECK_EQ(maps.size(), 2043u);
  }
}
BENCHMARK(BM_ReadMapFile);

static void BM_unwindstack_FileMaps(benchmark::State& state) {
  std::string map_file = android::base::GetExecutableDirectory() + "/testdata/maps";
  for (auto _ : state) {
    unwindstack::FileMaps maps(map_file);
    maps.Parse();
    CHECK_EQ(maps.Total(), 2043u);
  }
}
BENCHMARK(BM_unwindstack_FileMaps);

static void BM_unwindstack_BufferMaps(benchmark::State& state) {
  std::string map_file = android::base::GetExecutableDirectory() + "/testdata/maps";
  std::string content;
  CHECK(android::base::ReadFileToString(map_file, &content));
  for (auto _ : state) {
    unwindstack::BufferMaps maps(content.c_str());
    maps.Parse();
    CHECK_EQ(maps.Total(), 2043u);
  }
}
BENCHMARK(BM_unwindstack_BufferMaps);

static void BM_backtrace_BacktraceMap(benchmark::State& state) {
  pid_t pid = getpid();
  for (auto _ : state) {
    BacktraceMap* map = BacktraceMap::Create(pid, true);
    CHECK(map != nullptr);
    delete map;
  }
}
BENCHMARK(BM_backtrace_BacktraceMap);

BENCHMARK_MAIN();
