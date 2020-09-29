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

#define _GNU_SOURCE 1
#include <elf.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <type_traits>
#include <cinttypes>
#include <fstream>
#include <fcntl.h>

#include <algorithm>

#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include <unwindstack/Elf.h>
#include <unwindstack/JitDebug.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Unwinder.h>
#include <unwindstack/EnhanceDlsym.h>
#include <libgen.h>
#include <android/log.h>
#include "FpFallbackUnwinder.h"
#include "FpUnwinder.h"
#include "../libunwindstack/TimeUtil.h"

#include <unwindstack/DexFiles.h>

namespace wechat_backtrace {



static inline bool EndWith(std::string const &value, std::string const &ending) {
  if (ending.size() > value.size()) {
    return false;
  }
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

static inline void SkipDexPC() {

  uintptr_t map_base_addr;
  uintptr_t map_end_addr;
  char      map_perm[5];

  std::ifstream    input("/proc/self/maps");
  std::string aline;

  while (getline(input, aline)) {
    if (sscanf(aline.c_str(),
               "%" PRIxPTR "-%" PRIxPTR " %4s",
               &map_base_addr,
               &map_end_addr,
               map_perm) != 3) {
      continue;
    }

    if ('r' == map_perm[0]
        && 'x' == map_perm[2]
        && 'p' == map_perm[3]
        &&
        (EndWith(aline, ".dex")
         || EndWith(aline, ".odex")
         || EndWith(aline, ".vdex"))) {

      GetSkipFunctions()->push_back({map_base_addr, map_end_addr});
    }
  }
}

void UpdateFallbackPCRange() {

  GetSkipFunctions()->clear();

  void *handle = unwindstack::enhance::dlopen("libart.so", 0);

  if (handle) {

    void *trampoline =
            unwindstack::enhance::dlsym(handle, "art_quick_generic_jni_trampoline");

    if (nullptr != trampoline) {
      GetSkipFunctions()->push_back({reinterpret_cast<uintptr_t>(trampoline),
                                     reinterpret_cast<uintptr_t>(trampoline) + 0x27C});
    }

    void *art_quick_invoke_static_stub = unwindstack::enhance::dlsym(
            handle,
            "art_quick_invoke_static_stub");
    if (nullptr != art_quick_invoke_static_stub) {
      GetSkipFunctions()->push_back(
              {reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub),
               reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub) + 0x280});
    }
  }
  unwindstack::enhance::dlclose(handle);

  SkipDexPC();
}

} // wechat_backtrace

namespace unwindstack {

void FpFallbackUnwinder::fallbackUnwindFrame(bool &finished) {
//  frames_.clear();
  last_error_.code = ERROR_NONE;
  last_error_.address = 0;
//  elf_from_memory_not_file_ = false;

  fallbackUnwindFrameImpl(finished, false);

}
void FpFallbackUnwinder::fallbackUnwindFrameImpl(bool &finished, bool return_address_attempt) {

  ArchEnum arch = regs_->Arch();

//  bool return_address_attempt = false;
//  bool adjust_pc = true;
//  for (; frames_.size() < max_frames_;) {
    uint64_t cur_pc = regs_->pc();
    uint64_t cur_sp = regs_->sp();

    MapInfo* map_info = maps_->Find(regs_->pc());
    uint64_t pc_adjustment = 0;
    uint64_t step_pc;
    uint64_t rel_pc;
    Elf* elf;
    if (map_info == nullptr) {
      step_pc = regs_->pc();
//      rel_pc = step_pc;
      last_error_.code = ERROR_INVALID_MAP;
    } else {
//      if (ShouldStop(map_suffixes_to_ignore, map_info->name)) {
//        break;
//      }
      elf = map_info->GetElf(process_memory_, arch);
//      // If this elf is memory backed, and there is a valid file, then set
//      // an indicator that we couldn't open the file.
//      if (!elf_from_memory_not_file_ && map_info->memory_backed_elf && !map_info->name.empty() &&
//          map_info->name[0] != '[' && !android::base::StartsWith(map_info->name, "/memfd:")) {
//        elf_from_memory_not_file_ = true;
//      }
      step_pc = regs_->pc();
      rel_pc = elf->GetRelPc(step_pc, map_info);
      // Everyone except elf data in gdb jit debug maps uses the relative pc.
      if (!(map_info->flags & MAPS_FLAGS_JIT_SYMFILE_MAP)) {
        step_pc = rel_pc;
      }
//      if (adjust_pc) {
        pc_adjustment = GetPcAdjustment(rel_pc, elf, arch);
//      } else {
//        pc_adjustment = 0;
//      }
      step_pc -= pc_adjustment;

      // If the pc is in an invalid elf file, try and get an Elf object
      // using the jit debug information.
      if (!elf->valid() && jit_debug_ != nullptr) {
        uint64_t adjusted_jit_pc = regs_->pc() - pc_adjustment;
        Elf* jit_elf = jit_debug_->GetElf(maps_, adjusted_jit_pc);
        if (jit_elf != nullptr) {
          // The jit debug information requires a non relative adjusted pc.
          step_pc = adjusted_jit_pc;
          elf = jit_elf;
        }
      }
    }

//    FrameData* frame = nullptr;
    if (map_info == nullptr /* || initial_map_names_to_skip == nullptr ||
        std::find(initial_map_names_to_skip->begin(), initial_map_names_to_skip->end(),
                  basename(map_info->name.c_str())) == initial_map_names_to_skip->end() */) {
      if (regs_->dex_pc() != 0) {
        // Add a frame to represent the dex file.
        FillInDexFrame();
        // Clear the dex pc so that we don't repeat this frame later.
        regs_->set_dex_pc(0);

//        // Make sure there is enough room for the real frame.
//        if (frames_.size() == max_frames_) {  // TODO
//          last_error_.code = ERROR_MAX_FRAMES_EXCEEDED;
//          break;
//        }
      }

//      frame = FillInFrame(map_info, elf, rel_pc, pc_adjustment);

//      // Once a frame is added, stop skipping frames.
//      initial_map_names_to_skip = nullptr;
    }
//    adjust_pc = true;

    bool stepped = false;
    bool in_device_map = false;
    finished = false;
    if (map_info != nullptr) {
      if (map_info->flags & MAPS_FLAGS_DEVICE_MAP) {
        // Do not stop here, fall through in case we are
        // in the speculative unwind path and need to remove
        // some of the speculative frames.
        in_device_map = true;
      } else {
        MapInfo* sp_info = maps_->Find(regs_->sp());
        if (sp_info != nullptr && sp_info->flags & MAPS_FLAGS_DEVICE_MAP) {
          // Do not stop here, fall through in case we are
          // in the speculative unwind path and need to remove
          // some of the speculative frames.
          in_device_map = true;
        } else {
//          if (elf->StepIfSignalHandler(rel_pc, regs_, process_memory_.get())) {
//            stepped = true;
//            if (frame != nullptr) {
//              // Need to adjust the relative pc because the signal handler
//              // pc should not be adjusted.
//              frame->rel_pc = rel_pc;
//              frame->pc += pc_adjustment;
//              step_pc = rel_pc;
//            }
//          } else
          if (elf->Step(step_pc, regs_, process_memory_.get(), &finished)) {
            stepped = true;
          }
          elf->GetLastError(&last_error_);
        }
      }
    }

//    if (frame != nullptr) {
//      if (!resolve_names_ ||
//          !elf->GetFunctionName(step_pc, &frame->function_name, &frame->function_offset)) {
//        frame->function_name = "";
//        frame->function_offset = 0;
//      }
//    }

    if (finished) {
//      break;
      return;
    }

    if (!stepped) {
      if (return_address_attempt) {
//        // Only remove the speculative frame if there are more than two frames
//        // or the pc in the first frame is in a valid map.
//        // This allows for a case where the code jumps into the middle of
//        // nowhere, but there is no other unwind information after that.
//        if (frames_.size() > 2 || (frames_.size() > 0 && maps_->Find(frames_[0].pc) != nullptr)) {
//          // Remove the speculative frame.
//          frames_.pop_back();
//        }
//        break;
        finished = true;
        return;
      } else if (in_device_map) {
        // Do not attempt any other unwinding, pc or sp is in a device
        // map.
//        break;
        finished = true;
        return;
      } else {
        // Steping didn't work, try this secondary method.
        if (!regs_->SetPcFromReturnAddress(process_memory_.get())) {
//          break;
          return;
        }

        // try again
        fallbackUnwindFrameImpl(finished, true);
//        return_address_attempt = true;
      }
    }
//    else {
//      return_address_attempt = false;
//      if (max_frames_ == frames_.size()) {
//        last_error_.code = ERROR_MAX_FRAMES_EXCEEDED;
//      }
//    }

    // If the pc and sp didn't change, then consider everything stopped.
    if (cur_pc == regs_->pc() && cur_sp == regs_->sp()) {
      last_error_.code = ERROR_REPEATED_FRAME;
//      break;
      finished = true;
      return;
    }
//  }
}

}  // namespace unwindstack
