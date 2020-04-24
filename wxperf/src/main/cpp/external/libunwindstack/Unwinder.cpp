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

#include <algorithm>

#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include <demangle.h>

#include <unwindstack/Elf.h>
#include <unwindstack/JitDebug.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Unwinder.h>

#if !defined(NO_LIBDEXFILE_SUPPORT)
#include <unwindstack/DexFiles.h>
#endif

namespace unwindstack {

// Inject extra 'virtual' frame that represents the dex pc data.
// The dex pc is a magic register defined in the Mterp interpreter,
// and thus it will be restored/observed in the frame after it.
// Adding the dex frame first here will create something like:
//   #7 pc 0015fa20 core.vdex   java.util.Arrays.binarySearch+8
//   #8 pc 006b1ba1 libartd.so  ExecuteMterpImpl+14625
//   #9 pc 0039a1ef libartd.so  art::interpreter::Execute+719
void Unwinder::FillInDexFrame() {
  size_t frame_num = frames_.size();
  frames_.resize(frame_num + 1);
  FrameData* frame = &frames_.at(frame_num);
  frame->num = frame_num;

  uint64_t dex_pc = regs_->dex_pc();
  frame->pc = dex_pc;
  frame->sp = regs_->sp();

  MapInfo* info = maps_->Find(dex_pc);
  if (info != nullptr) {
    frame->map_start = info->start;
    frame->map_end = info->end;
    frame->map_elf_start_offset = info->elf_start_offset;
    frame->map_exact_offset = info->offset;
    frame->map_load_bias = info->load_bias;
    frame->map_flags = info->flags;
    if (resolve_names_) {
      frame->map_name = info->name;
    }
    frame->rel_pc = dex_pc - info->start;
  } else {
    frame->rel_pc = dex_pc;
    return;
  }

  if (!resolve_names_) {
    return;
  }

#if !defined(NO_LIBDEXFILE_SUPPORT)
  if (dex_files_ == nullptr) {
    return;
  }

  dex_files_->GetMethodInformation(maps_, info, dex_pc, &frame->function_name,
                                   &frame->function_offset);
#endif
}

FrameData* Unwinder::FillInFrame(MapInfo* map_info, Elf* elf, uint64_t rel_pc,
                                 uint64_t pc_adjustment) {
  size_t frame_num = frames_.size();
  frames_.resize(frame_num + 1);
  FrameData* frame = &frames_.at(frame_num);
  frame->num = frame_num;
  frame->sp = regs_->sp();
  frame->rel_pc = rel_pc - pc_adjustment;
  frame->pc = regs_->pc() - pc_adjustment;

  if (map_info == nullptr) {
    // Nothing else to update.
    return nullptr;
  }

  if (resolve_names_) {
    frame->map_name = map_info->name;
    if (embedded_soname_ && map_info->elf_start_offset != 0 && !frame->map_name.empty()) {
      std::string soname = elf->GetSoname();
      if (!soname.empty()) {
        frame->map_name += '!' + soname;
      }
    }
  }
  frame->map_elf_start_offset = map_info->elf_start_offset;
  frame->map_exact_offset = map_info->offset;
  frame->map_start = map_info->start;
  frame->map_end = map_info->end;
  frame->map_flags = map_info->flags;
  frame->map_load_bias = elf->GetLoadBias();
  return frame;
}

static bool ShouldStop(const std::vector<std::string>* map_suffixes_to_ignore,
                       std::string& map_name) {
  if (map_suffixes_to_ignore == nullptr) {
    return false;
  }
  auto pos = map_name.find_last_of('.');
  if (pos == std::string::npos) {
    return false;
  }

  return std::find(map_suffixes_to_ignore->begin(), map_suffixes_to_ignore->end(),
                   map_name.substr(pos + 1)) != map_suffixes_to_ignore->end();
}

void Unwinder::Unwind(const std::vector<std::string>* initial_map_names_to_skip,
                      const std::vector<std::string>* map_suffixes_to_ignore) {
  frames_.clear();
  last_error_.code = ERROR_NONE;
  last_error_.address = 0;
  elf_from_memory_not_file_ = false;

  ArchEnum arch = regs_->Arch();

  bool return_address_attempt = false;
  bool adjust_pc = false;
  for (; frames_.size() < max_frames_;) {
    uint64_t cur_pc = regs_->pc();
    uint64_t cur_sp = regs_->sp();

    MapInfo* map_info = maps_->Find(regs_->pc());
    uint64_t pc_adjustment = 0;
    uint64_t step_pc;
    uint64_t rel_pc;
    Elf* elf;
    if (map_info == nullptr) {
      step_pc = regs_->pc();
      rel_pc = step_pc;
      last_error_.code = ERROR_INVALID_MAP;
    } else {
      if (ShouldStop(map_suffixes_to_ignore, map_info->name)) {
        break;
      }
      elf = map_info->GetElf(process_memory_, arch);
      // If this elf is memory backed, and there is a valid file, then set
      // an indicator that we couldn't open the file.
      if (!elf_from_memory_not_file_ && map_info->memory_backed_elf && !map_info->name.empty() &&
          map_info->name[0] != '[' && !android::base::StartsWith(map_info->name, "/memfd:")) {
        elf_from_memory_not_file_ = true;
      }
      step_pc = regs_->pc();
      rel_pc = elf->GetRelPc(step_pc, map_info);
      // Everyone except elf data in gdb jit debug maps uses the relative pc.
      if (!(map_info->flags & MAPS_FLAGS_JIT_SYMFILE_MAP)) {
        step_pc = rel_pc;
      }
      if (adjust_pc) {
        pc_adjustment = regs_->GetPcAdjustment(rel_pc, elf);
      } else {
        pc_adjustment = 0;
      }
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

    FrameData* frame = nullptr;
    if (map_info == nullptr || initial_map_names_to_skip == nullptr ||
        std::find(initial_map_names_to_skip->begin(), initial_map_names_to_skip->end(),
                  basename(map_info->name.c_str())) == initial_map_names_to_skip->end()) {
      if (regs_->dex_pc() != 0) {
        // Add a frame to represent the dex file.
        FillInDexFrame();
        // Clear the dex pc so that we don't repeat this frame later.
        regs_->set_dex_pc(0);

        // Make sure there is enough room for the real frame.
        if (frames_.size() == max_frames_) {
          last_error_.code = ERROR_MAX_FRAMES_EXCEEDED;
          break;
        }
      }

      frame = FillInFrame(map_info, elf, rel_pc, pc_adjustment);

      // Once a frame is added, stop skipping frames.
      initial_map_names_to_skip = nullptr;
    }
    adjust_pc = true;

    bool stepped = false;
    bool in_device_map = false;
    bool finished = false;
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
          if (elf->StepIfSignalHandler(rel_pc, regs_, process_memory_.get())) {
            stepped = true;
            if (frame != nullptr) {
              // Need to adjust the relative pc because the signal handler
              // pc should not be adjusted.
              frame->rel_pc = rel_pc;
              frame->pc += pc_adjustment;
              step_pc = rel_pc;
            }
          } else if (elf->Step(step_pc, regs_, process_memory_.get(), &finished)) {
            stepped = true;
          }
          elf->GetLastError(&last_error_);
        }
      }
    }

    if (frame != nullptr) {
      if (!resolve_names_ ||
          !elf->GetFunctionName(step_pc, &frame->function_name, &frame->function_offset)) {
        frame->function_name = "";
        frame->function_offset = 0;
      }
    }

    if (finished) {
      break;
    }

    if (!stepped) {
      if (return_address_attempt) {
        // Only remove the speculative frame if there are more than two frames
        // or the pc in the first frame is in a valid map.
        // This allows for a case where the code jumps into the middle of
        // nowhere, but there is no other unwind information after that.
        if (frames_.size() > 2 || (frames_.size() > 0 && maps_->Find(frames_[0].pc) != nullptr)) {
          // Remove the speculative frame.
          frames_.pop_back();
        }
        break;
      } else if (in_device_map) {
        // Do not attempt any other unwinding, pc or sp is in a device
        // map.
        break;
      } else {
        // Steping didn't work, try this secondary method.
        if (!regs_->SetPcFromReturnAddress(process_memory_.get())) {
          break;
        }
        return_address_attempt = true;
      }
    } else {
      return_address_attempt = false;
      if (max_frames_ == frames_.size()) {
        last_error_.code = ERROR_MAX_FRAMES_EXCEEDED;
      }
    }

    // If the pc and sp didn't change, then consider everything stopped.
    if (cur_pc == regs_->pc() && cur_sp == regs_->sp()) {
      last_error_.code = ERROR_REPEATED_FRAME;
      break;
    }
  }
}

std::string Unwinder::FormatFrame(const FrameData& frame) {
  std::string data;
  if (regs_->Is32Bit()) {
    data += android::base::StringPrintf("  #%02zu pc %08" PRIx64, frame.num, frame.rel_pc);
  } else {
    data += android::base::StringPrintf("  #%02zu pc %016" PRIx64, frame.num, frame.rel_pc);
  }

  if (frame.map_start == frame.map_end) {
    // No valid map associated with this frame.
    data += "  <unknown>";
  } else if (!frame.map_name.empty()) {
    data += "  " + frame.map_name;
  } else {
    data += android::base::StringPrintf("  <anonymous:%" PRIx64 ">", frame.map_start);
  }

  if (frame.map_elf_start_offset != 0) {
    data += android::base::StringPrintf(" (offset 0x%" PRIx64 ")", frame.map_elf_start_offset);
  }

  if (!frame.function_name.empty()) {
    data += " (" + demangle(frame.function_name.c_str());
    if (frame.function_offset != 0) {
      data += android::base::StringPrintf("+%" PRId64, frame.function_offset);
    }
    data += ')';
  }

  MapInfo* map_info = maps_->Find(frame.map_start);
  if (map_info != nullptr && display_build_id_) {
    std::string build_id = map_info->GetPrintableBuildID();
    if (!build_id.empty()) {
      data += " (BuildId: " + build_id + ')';
    }
  }
  return data;
}

std::string Unwinder::FormatFrame(size_t frame_num) {
  if (frame_num >= frames_.size()) {
    return "";
  }
  return FormatFrame(frames_[frame_num]);
}

void Unwinder::SetJitDebug(JitDebug* jit_debug, ArchEnum arch) {
  jit_debug->SetArch(arch);
  jit_debug_ = jit_debug;
}

#if !defined(NO_LIBDEXFILE_SUPPORT)
void Unwinder::SetDexFiles(DexFiles* dex_files, ArchEnum arch) {
  dex_files->SetArch(arch);
  dex_files_ = dex_files;
}
#endif

bool UnwinderFromPid::Init(ArchEnum arch) {
  if (pid_ == getpid()) {
    maps_ptr_.reset(new LocalMaps());
  } else {
    maps_ptr_.reset(new RemoteMaps(pid_));
  }
  if (!maps_ptr_->Parse()) {
    return false;
  }
  maps_ = maps_ptr_.get();

  process_memory_ = Memory::CreateProcessMemoryCached(pid_);

  jit_debug_ptr_.reset(new JitDebug(process_memory_));
  jit_debug_ = jit_debug_ptr_.get();
  SetJitDebug(jit_debug_, arch);
#if !defined(NO_LIBDEXFILE_SUPPORT)
  dex_files_ptr_.reset(new DexFiles(process_memory_));
  dex_files_ = dex_files_ptr_.get();
  SetDexFiles(dex_files_, arch);
#endif

  return true;
}

}  // namespace unwindstack
