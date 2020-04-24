/*
 * Copyright (C) 2018 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <pthread.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <unwindstack/Elf.h>
#include <unwindstack/LocalUnwinder.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsGetLocal.h>

namespace unwindstack {

bool LocalUnwinder::Init() {
  pthread_rwlock_init(&maps_rwlock_, nullptr);

  // Create the maps.
  maps_.reset(new unwindstack::LocalUpdatableMaps());
  if (!maps_->Parse()) {
    maps_.reset();
    return false;
  }

  process_memory_ = unwindstack::Memory::CreateProcessMemory(getpid());

  return true;
}

bool LocalUnwinder::ShouldSkipLibrary(const std::string& map_name) {
  for (const std::string& skip_library : skip_libraries_) {
    if (skip_library == map_name) {
      return true;
    }
  }
  return false;
}

MapInfo* LocalUnwinder::GetMapInfo(uint64_t pc) {
  pthread_rwlock_rdlock(&maps_rwlock_);
  MapInfo* map_info = maps_->Find(pc);
  pthread_rwlock_unlock(&maps_rwlock_);

  if (map_info == nullptr) {
    pthread_rwlock_wrlock(&maps_rwlock_);
    // This is guaranteed not to invalidate any previous MapInfo objects so
    // we don't need to worry about any MapInfo* values already in use.
    if (maps_->Reparse()) {
      map_info = maps_->Find(pc);
    }
    pthread_rwlock_unlock(&maps_rwlock_);
  }

  return map_info;
}

bool LocalUnwinder::Unwind(std::vector<LocalFrameData>* frame_info, size_t max_frames) {
  std::unique_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
  unwindstack::RegsGetLocal(regs.get());
  ArchEnum arch = regs->Arch();

  size_t num_frames = 0;
  bool adjust_pc = false;
  while (true) {
    uint64_t cur_pc = regs->pc();
    uint64_t cur_sp = regs->sp();

    MapInfo* map_info = GetMapInfo(cur_pc);
    if (map_info == nullptr) {
      break;
    }

    Elf* elf = map_info->GetElf(process_memory_, arch);
    uint64_t rel_pc = elf->GetRelPc(cur_pc, map_info);
    uint64_t step_pc = rel_pc;
    uint64_t pc_adjustment;
    if (adjust_pc) {
      pc_adjustment = regs->GetPcAdjustment(rel_pc, elf);
    } else {
      pc_adjustment = 0;
    }
    step_pc -= pc_adjustment;

    bool finished = false;
    if (elf->StepIfSignalHandler(rel_pc, regs.get(), process_memory_.get())) {
      step_pc = rel_pc;
    } else if (!elf->Step(step_pc, regs.get(), process_memory_.get(), &finished)) {
      finished = true;
    }

    // Skip any locations that are within this library.
    if (num_frames != 0 || !ShouldSkipLibrary(map_info->name)) {
      // Add frame information.
      std::string func_name;
      uint64_t func_offset;
      if (elf->GetFunctionName(rel_pc, &func_name, &func_offset)) {
        frame_info->emplace_back(map_info, cur_pc - pc_adjustment, rel_pc - pc_adjustment,
                                 func_name, func_offset);
      } else {
        frame_info->emplace_back(map_info, cur_pc - pc_adjustment, rel_pc - pc_adjustment, "", 0);
      }
      num_frames++;
    }

    if (finished || frame_info->size() == max_frames ||
        (cur_pc == regs->pc() && cur_sp == regs->sp())) {
      break;
    }
    adjust_pc = true;
  }
  return num_frames != 0;
}

}  // namespace unwindstack
