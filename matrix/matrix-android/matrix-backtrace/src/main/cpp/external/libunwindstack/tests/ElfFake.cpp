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

#include <stdint.h>

#include <deque>
#include <string>

#include <unwindstack/Elf.h>
#include <unwindstack/ElfInterface.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>

#include "ElfFake.h"
#include "RegsFake.h"

namespace unwindstack {

std::deque<FunctionData> ElfInterfaceFake::functions_;
std::deque<StepData> ElfInterfaceFake::steps_;

bool ElfInterfaceFake::GetFunctionName(uint64_t, std::string* name, uint64_t* offset) {
  if (functions_.empty()) {
    return false;
  }
  auto entry = functions_.front();
  functions_.pop_front();
  *name = entry.name;
  *offset = entry.offset;
  return true;
}

bool ElfInterfaceFake::GetGlobalVariable(const std::string& global, uint64_t* offset) {
  auto entry = globals_.find(global);
  if (entry == globals_.end()) {
    return false;
  }
  *offset = entry->second;
  return true;
}

bool ElfInterfaceFake::Step(uint64_t, Regs* regs, Memory*, bool* finished) {
  if (steps_.empty()) {
    return false;
  }
  auto entry = steps_.front();
  steps_.pop_front();

  if (entry.pc == 0 && entry.sp == 0 && !entry.finished) {
    // Pretend as though there is no frame.
    return false;
  }

  RegsFake* fake_regs = reinterpret_cast<RegsFake*>(regs);
  fake_regs->set_pc(entry.pc);
  fake_regs->set_sp(entry.sp);
  *finished = entry.finished;
  return true;
}

}  // namespace unwindstack
