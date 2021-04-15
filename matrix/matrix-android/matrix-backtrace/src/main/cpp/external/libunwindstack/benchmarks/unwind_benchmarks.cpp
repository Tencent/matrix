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

#include <stdint.h>

#include <memory>

#include <benchmark/benchmark.h>

#include <android-base/strings.h>

#include <unwindstack/Elf.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsGetLocal.h>
#include <unwindstack/Unwinder.h>

size_t Call6(std::shared_ptr<unwindstack::Memory>& process_memory, unwindstack::Maps* maps) {
  std::unique_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
  unwindstack::RegsGetLocal(regs.get());
  unwindstack::Unwinder unwinder(32, maps, regs.get(), process_memory);
  unwinder.Unwind();
  return unwinder.NumFrames();
}

size_t Call5(std::shared_ptr<unwindstack::Memory>& process_memory, unwindstack::Maps* maps) {
  return Call6(process_memory, maps);
}

size_t Call4(std::shared_ptr<unwindstack::Memory>& process_memory, unwindstack::Maps* maps) {
  return Call5(process_memory, maps);
}

size_t Call3(std::shared_ptr<unwindstack::Memory>& process_memory, unwindstack::Maps* maps) {
  return Call4(process_memory, maps);
}

size_t Call2(std::shared_ptr<unwindstack::Memory>& process_memory, unwindstack::Maps* maps) {
  return Call3(process_memory, maps);
}

size_t Call1(std::shared_ptr<unwindstack::Memory>& process_memory, unwindstack::Maps* maps) {
  return Call2(process_memory, maps);
}

static void BM_uncached_unwind(benchmark::State& state) {
  auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
  unwindstack::LocalMaps maps;
  if (!maps.Parse()) {
    state.SkipWithError("Failed to parse local maps.");
  }

  for (auto _ : state) {
    benchmark::DoNotOptimize(Call1(process_memory, &maps));
  }
}
BENCHMARK(BM_uncached_unwind);

static void BM_cached_unwind(benchmark::State& state) {
  auto process_memory = unwindstack::Memory::CreateProcessMemoryCached(getpid());
  unwindstack::LocalMaps maps;
  if (!maps.Parse()) {
    state.SkipWithError("Failed to parse local maps.");
  }

  for (auto _ : state) {
    benchmark::DoNotOptimize(Call1(process_memory, &maps));
  }
}
BENCHMARK(BM_cached_unwind);

static void Initialize(benchmark::State& state, unwindstack::Maps& maps,
                       unwindstack::MapInfo** build_id_map_info) {
  if (!maps.Parse()) {
    state.SkipWithError("Failed to parse local maps.");
    return;
  }

  // Find the libc.so share library and use that for benchmark purposes.
  *build_id_map_info = nullptr;
  for (auto& map_info : maps) {
    if (map_info->offset == 0 && map_info->GetBuildID() != "") {
      *build_id_map_info = map_info.get();
      break;
    }
  }

  if (*build_id_map_info == nullptr) {
    state.SkipWithError("Failed to find a map with a BuildID.");
  }
}

static void BM_get_build_id_from_elf(benchmark::State& state) {
  unwindstack::LocalMaps maps;
  unwindstack::MapInfo* build_id_map_info;
  Initialize(state, maps, &build_id_map_info);

  unwindstack::Elf* elf = build_id_map_info->GetElf(std::shared_ptr<unwindstack::Memory>(),
                                                    unwindstack::Regs::CurrentArch());
  if (!elf->valid()) {
    state.SkipWithError("Cannot get valid elf from map.");
  }

  for (auto _ : state) {
    uintptr_t id = build_id_map_info->build_id;
    if (id != 0) {
      delete reinterpret_cast<std::string*>(id);
      build_id_map_info->build_id = 0;
    }
    benchmark::DoNotOptimize(build_id_map_info->GetBuildID());
  }
}
BENCHMARK(BM_get_build_id_from_elf);

static void BM_get_build_id_from_file(benchmark::State& state) {
  unwindstack::LocalMaps maps;
  unwindstack::MapInfo* build_id_map_info;
  Initialize(state, maps, &build_id_map_info);

  for (auto _ : state) {
    uintptr_t id = build_id_map_info->build_id;
    if (id != 0) {
      delete reinterpret_cast<std::string*>(id);
      build_id_map_info->build_id = 0;
    }
    benchmark::DoNotOptimize(build_id_map_info->GetBuildID());
  }
}
BENCHMARK(BM_get_build_id_from_file);

BENCHMARK_MAIN();
