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

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfLocation.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/DwarfSection.h>
#include <unwindstack/DwarfStructs.h>
#include <unwindstack/Log.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>

#include "DwarfCfa.h"
#include "DwarfDebugFrame.h"
#include "DwarfEhFrame.h"
#include "DwarfEncoding.h"
#include "DwarfOp.h"
#include "RegsInfo.h"

namespace unwindstack {

DwarfSection::DwarfSection(Memory* memory) : memory_(memory) {}

bool DwarfSection::Step(uint64_t pc, Regs* regs, Memory* process_memory, bool* finished) {
  // Lookup the pc in the cache.
  auto it = loc_regs_.upper_bound(pc);
  if (it == loc_regs_.end() || pc < it->second.pc_start) {
    last_error_.code = DWARF_ERROR_NONE;
    const DwarfFde* fde = GetFdeFromPc(pc);
    if (fde == nullptr || fde->cie == nullptr) {
      last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
      return false;
    }

    // Now get the location information for this pc.
    dwarf_loc_regs_t loc_regs;
    if (!GetCfaLocationInfo(pc, fde, &loc_regs)) {
      return false;
    }
    loc_regs.cie = fde->cie;

    // Store it in the cache.
    it = loc_regs_.emplace(loc_regs.pc_end, std::move(loc_regs)).first;
  }

  // Now eval the actual registers.
  return Eval(it->second.cie, process_memory, it->second, regs, finished);
}

template <typename AddressType>
const DwarfCie* DwarfSectionImpl<AddressType>::GetCieFromOffset(uint64_t offset) {
  auto cie_entry = cie_entries_.find(offset);
  if (cie_entry != cie_entries_.end()) {
    return &cie_entry->second;
  }
  DwarfCie* cie = &cie_entries_[offset];
  memory_.set_data_offset(entries_offset_);
  memory_.set_cur_offset(offset);
  if (!FillInCieHeader(cie) || !FillInCie(cie)) {
    // Erase the cached entry.
    cie_entries_.erase(offset);
    return nullptr;
  }
  return cie;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::FillInCieHeader(DwarfCie* cie) {
  cie->lsda_encoding = DW_EH_PE_omit;
  uint32_t length32;
  if (!memory_.ReadBytes(&length32, sizeof(length32))) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }
  if (length32 == static_cast<uint32_t>(-1)) {
    // 64 bit Cie
    uint64_t length64;
    if (!memory_.ReadBytes(&length64, sizeof(length64))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }

    cie->cfa_instructions_end = memory_.cur_offset() + length64;
    cie->fde_address_encoding = DW_EH_PE_sdata8;

    uint64_t cie_id;
    if (!memory_.ReadBytes(&cie_id, sizeof(cie_id))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    if (cie_id != cie64_value_) {
      // This is not a Cie, something has gone horribly wrong.
      last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
      return false;
    }
  } else {
    // 32 bit Cie
    cie->cfa_instructions_end = memory_.cur_offset() + length32;
    cie->fde_address_encoding = DW_EH_PE_sdata4;

    uint32_t cie_id;
    if (!memory_.ReadBytes(&cie_id, sizeof(cie_id))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    if (cie_id != cie32_value_) {
      // This is not a Cie, something has gone horribly wrong.
      last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
      return false;
    }
  }
  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::FillInCie(DwarfCie* cie) {
  if (!memory_.ReadBytes(&cie->version, sizeof(cie->version))) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  if (cie->version != 1 && cie->version != 3 && cie->version != 4 && cie->version != 5) {
    // Unrecognized version.
    last_error_.code = DWARF_ERROR_UNSUPPORTED_VERSION;
    return false;
  }

  // Read the augmentation string.
  char aug_value;
  do {
    if (!memory_.ReadBytes(&aug_value, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    cie->augmentation_string.push_back(aug_value);
  } while (aug_value != '\0');

  if (cie->version == 4 || cie->version == 5) {
    // Skip the Address Size field since we only use it for validation.
    memory_.set_cur_offset(memory_.cur_offset() + 1);

    // Segment Size
    if (!memory_.ReadBytes(&cie->segment_size, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
  }

  // Code Alignment Factor
  if (!memory_.ReadULEB128(&cie->code_alignment_factor)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  // Data Alignment Factor
  if (!memory_.ReadSLEB128(&cie->data_alignment_factor)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  if (cie->version == 1) {
    // Return Address is a single byte.
    uint8_t return_address_register;
    if (!memory_.ReadBytes(&return_address_register, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    cie->return_address_register = return_address_register;
  } else if (!memory_.ReadULEB128(&cie->return_address_register)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  if (cie->augmentation_string[0] != 'z') {
    cie->cfa_instructions_offset = memory_.cur_offset();
    return true;
  }

  uint64_t aug_length;
  if (!memory_.ReadULEB128(&aug_length)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }
  cie->cfa_instructions_offset = memory_.cur_offset() + aug_length;

  for (size_t i = 1; i < cie->augmentation_string.size(); i++) {
    switch (cie->augmentation_string[i]) {
      case 'L':
        if (!memory_.ReadBytes(&cie->lsda_encoding, 1)) {
          last_error_.code = DWARF_ERROR_MEMORY_INVALID;
          last_error_.address = memory_.cur_offset();
          return false;
        }
        break;
      case 'P': {
        uint8_t encoding;
        if (!memory_.ReadBytes(&encoding, 1)) {
          last_error_.code = DWARF_ERROR_MEMORY_INVALID;
          last_error_.address = memory_.cur_offset();
          return false;
        }
        memory_.set_pc_offset(pc_offset_);
        if (!memory_.ReadEncodedValue<AddressType>(encoding, &cie->personality_handler)) {
          last_error_.code = DWARF_ERROR_MEMORY_INVALID;
          last_error_.address = memory_.cur_offset();
          return false;
        }
      } break;
      case 'R':
        if (!memory_.ReadBytes(&cie->fde_address_encoding, 1)) {
          last_error_.code = DWARF_ERROR_MEMORY_INVALID;
          last_error_.address = memory_.cur_offset();
          return false;
        }
        break;
    }
  }
  return true;
}

template <typename AddressType>
const DwarfFde* DwarfSectionImpl<AddressType>::GetFdeFromOffset(uint64_t offset) {
  auto fde_entry = fde_entries_.find(offset);
  if (fde_entry != fde_entries_.end()) {
    return &fde_entry->second;
  }
  DwarfFde* fde = &fde_entries_[offset];
  memory_.set_data_offset(entries_offset_);
  memory_.set_cur_offset(offset);
  if (!FillInFdeHeader(fde) || !FillInFde(fde)) {
    fde_entries_.erase(offset);
    return nullptr;
  }
  return fde;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::FillInFdeHeader(DwarfFde* fde) {
  uint32_t length32;
  if (!memory_.ReadBytes(&length32, sizeof(length32))) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  if (length32 == static_cast<uint32_t>(-1)) {
    // 64 bit Fde.
    uint64_t length64;
    if (!memory_.ReadBytes(&length64, sizeof(length64))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    fde->cfa_instructions_end = memory_.cur_offset() + length64;

    uint64_t value64;
    if (!memory_.ReadBytes(&value64, sizeof(value64))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    if (value64 == cie64_value_) {
      // This is a Cie, this means something has gone wrong.
      last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
      return false;
    }

    // Get the Cie pointer, which is necessary to properly read the rest of
    // of the Fde information.
    fde->cie_offset = GetCieOffsetFromFde64(value64);
  } else {
    // 32 bit Fde.
    fde->cfa_instructions_end = memory_.cur_offset() + length32;

    uint32_t value32;
    if (!memory_.ReadBytes(&value32, sizeof(value32))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    if (value32 == cie32_value_) {
      // This is a Cie, this means something has gone wrong.
      last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
      return false;
    }

    // Get the Cie pointer, which is necessary to properly read the rest of
    // of the Fde information.
    fde->cie_offset = GetCieOffsetFromFde32(value32);
  }
  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::FillInFde(DwarfFde* fde) {
  uint64_t cur_offset = memory_.cur_offset();

  const DwarfCie* cie = GetCieFromOffset(fde->cie_offset);
  if (cie == nullptr) {
    return false;
  }
  fde->cie = cie;

  if (cie->segment_size != 0) {
    // Skip over the segment selector for now.
    cur_offset += cie->segment_size;
  }
  memory_.set_cur_offset(cur_offset);

  // The load bias only applies to the start.
  memory_.set_pc_offset(section_bias_);
  bool valid = memory_.ReadEncodedValue<AddressType>(cie->fde_address_encoding, &fde->pc_start);
  fde->pc_start = AdjustPcFromFde(fde->pc_start);

  memory_.set_pc_offset(0);
  if (!valid || !memory_.ReadEncodedValue<AddressType>(cie->fde_address_encoding, &fde->pc_end)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }
  fde->pc_end += fde->pc_start;

  if (cie->augmentation_string.size() > 0 && cie->augmentation_string[0] == 'z') {
    // Augmentation Size
    uint64_t aug_length;
    if (!memory_.ReadULEB128(&aug_length)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    uint64_t cur_offset = memory_.cur_offset();

    memory_.set_pc_offset(pc_offset_);
    if (!memory_.ReadEncodedValue<AddressType>(cie->lsda_encoding, &fde->lsda_address)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }

    // Set our position to after all of the augmentation data.
    memory_.set_cur_offset(cur_offset + aug_length);
  }
  fde->cfa_instructions_offset = memory_.cur_offset();

  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::EvalExpression(const DwarfLocation& loc, Memory* regular_memory,
                                                   AddressType* value,
                                                   RegsInfo<AddressType>* regs_info,
                                                   bool* is_dex_pc) {
  DwarfOp<AddressType> op(&memory_, regular_memory);
  op.set_regs_info(regs_info);

  // Need to evaluate the op data.
  uint64_t end = loc.values[1];
  uint64_t start = end - loc.values[0];
  if (!op.Eval(start, end)) {
    last_error_ = op.last_error();
    return false;
  }
  if (op.StackSize() == 0) {
    last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
    return false;
  }
  // We don't support an expression that evaluates to a register number.
  if (op.is_register()) {
    last_error_.code = DWARF_ERROR_NOT_IMPLEMENTED;
    return false;
  }
  *value = op.StackAt(0);
  if (is_dex_pc != nullptr && op.dex_pc_set()) {
    *is_dex_pc = true;
  }
  return true;
}

template <typename AddressType>
struct EvalInfo {
  const dwarf_loc_regs_t* loc_regs;
  const DwarfCie* cie;
  Memory* regular_memory;
  AddressType cfa;
  bool return_address_undefined = false;
  RegsInfo<AddressType> regs_info;
};

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::EvalRegister(const DwarfLocation* loc, uint32_t reg,
                                                 AddressType* reg_ptr, void* info) {
  EvalInfo<AddressType>* eval_info = reinterpret_cast<EvalInfo<AddressType>*>(info);
  Memory* regular_memory = eval_info->regular_memory;
  switch (loc->type) {
    case DWARF_LOCATION_OFFSET:
      if (!regular_memory->ReadFully(eval_info->cfa + loc->values[0], reg_ptr, sizeof(AddressType))) {
        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
        last_error_.address = eval_info->cfa + loc->values[0];
        return false;
      }
      break;
    case DWARF_LOCATION_VAL_OFFSET:
      *reg_ptr = eval_info->cfa + loc->values[0];
      break;
    case DWARF_LOCATION_REGISTER: {
      uint32_t cur_reg = loc->values[0];
      if (cur_reg >= eval_info->regs_info.Total()) {
        last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
        return false;
      }
      *reg_ptr = eval_info->regs_info.Get(cur_reg) + loc->values[1];
      break;
    }
    case DWARF_LOCATION_EXPRESSION:
    case DWARF_LOCATION_VAL_EXPRESSION: {
      AddressType value;
      bool is_dex_pc = false;
      if (!EvalExpression(*loc, regular_memory, &value, &eval_info->regs_info, &is_dex_pc)) {
        return false;
      }
      if (loc->type == DWARF_LOCATION_EXPRESSION) {
        if (!regular_memory->ReadFully(value, reg_ptr, sizeof(AddressType))) {
          last_error_.code = DWARF_ERROR_MEMORY_INVALID;
          last_error_.address = value;
          return false;
        }
      } else {
        *reg_ptr = value;
        if (is_dex_pc) {
          eval_info->regs_info.regs->set_dex_pc(value);
        }
      }
      break;
    }
    case DWARF_LOCATION_UNDEFINED:
      if (reg == eval_info->cie->return_address_register) {
        eval_info->return_address_undefined = true;
      }
      break;
    default:
      break;
  }

  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::Eval(const DwarfCie* cie, Memory* regular_memory,
                                         const dwarf_loc_regs_t& loc_regs, Regs* regs,
                                         bool* finished) {
  RegsImpl<AddressType>* cur_regs = reinterpret_cast<RegsImpl<AddressType>*>(regs);
  if (cie->return_address_register >= cur_regs->total_regs()) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }

  // Get the cfa value;
  auto cfa_entry = loc_regs.find(CFA_REG);
  if (cfa_entry == loc_regs.end()) {
    last_error_.code = DWARF_ERROR_CFA_NOT_DEFINED;
    return false;
  }

  // Always set the dex pc to zero when evaluating.
  cur_regs->set_dex_pc(0);

  EvalInfo<AddressType> eval_info{.loc_regs = &loc_regs,
                                  .cie = cie,
                                  .regular_memory = regular_memory,
                                  .regs_info = RegsInfo<AddressType>(cur_regs)};
  const DwarfLocation* loc = &cfa_entry->second;
  // Only a few location types are valid for the cfa.
  switch (loc->type) {
    case DWARF_LOCATION_REGISTER:
      if (loc->values[0] >= cur_regs->total_regs()) {
        last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
        return false;
      }
      eval_info.cfa = (*cur_regs)[loc->values[0]];
      eval_info.cfa += loc->values[1];
      break;
    case DWARF_LOCATION_VAL_EXPRESSION: {
      AddressType value;
      if (!EvalExpression(*loc, regular_memory, &value, &eval_info.regs_info, nullptr)) {
        return false;
      }
      // There is only one type of valid expression for CFA evaluation.
      eval_info.cfa = value;
      break;
    }
    default:
      last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
      return false;
  }

  for (const auto& entry : loc_regs) {
    uint32_t reg = entry.first;
    // Already handled the CFA register.
    if (reg == CFA_REG) continue;

    AddressType* reg_ptr;
    if (reg >= cur_regs->total_regs()) {
      // Skip this unknown register.
      continue;
    }

    reg_ptr = eval_info.regs_info.Save(reg);
    if (!EvalRegister(&entry.second, reg, reg_ptr, &eval_info)) {
      return false;
    }
  }

  // Find the return address location.
  if (eval_info.return_address_undefined) {
    cur_regs->set_pc(0);
  } else {
    cur_regs->set_pc((*cur_regs)[cie->return_address_register]);
  }

  // If the pc was set to zero, consider this the final frame.
  *finished = (cur_regs->pc() == 0) ? true : false;

  cur_regs->set_sp(eval_info.cfa);

  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::GetCfaLocationInfo(uint64_t pc, const DwarfFde* fde,
                                                       dwarf_loc_regs_t* loc_regs) {
  DwarfCfa<AddressType> cfa(&memory_, fde);

  // Look for the cached copy of the cie data.
  auto reg_entry = cie_loc_regs_.find(fde->cie_offset);
  if (reg_entry == cie_loc_regs_.end()) {
    if (!cfa.GetLocationInfo(pc, fde->cie->cfa_instructions_offset, fde->cie->cfa_instructions_end,
                             loc_regs)) {
      last_error_ = cfa.last_error();
      return false;
    }
    cie_loc_regs_[fde->cie_offset] = *loc_regs;
  }
  cfa.set_cie_loc_regs(&cie_loc_regs_[fde->cie_offset]);
  if (!cfa.GetLocationInfo(pc, fde->cfa_instructions_offset, fde->cfa_instructions_end, loc_regs)) {
    last_error_ = cfa.last_error();
    return false;
  }
  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::Log(uint8_t indent, uint64_t pc, const DwarfFde* fde) {
  DwarfCfa<AddressType> cfa(&memory_, fde);

  // Always print the cie information.
  const DwarfCie* cie = fde->cie;
  if (!cfa.Log(indent, pc, cie->cfa_instructions_offset, cie->cfa_instructions_end)) {
    last_error_ = cfa.last_error();
    return false;
  }
  if (!cfa.Log(indent, pc, fde->cfa_instructions_offset, fde->cfa_instructions_end)) {
    last_error_ = cfa.last_error();
    return false;
  }
  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::Init(uint64_t offset, uint64_t size, int64_t section_bias) {
  section_bias_ = section_bias;
  entries_offset_ = offset;
  next_entries_offset_ = offset;
  entries_end_ = offset + size;

  memory_.clear_func_offset();
  memory_.clear_text_offset();
  memory_.set_cur_offset(offset);
  pc_offset_ = offset;

  return true;
}

// Create a cached version of the fde information such that it is a std::map
// that is indexed by end pc and contains a pair that represents the start pc
// followed by the fde object. The fde pointers are owned by fde_entries_
// and not by the map object.
// It is possible for an fde to be represented by multiple entries in
// the map. This can happen if the the start pc and end pc overlap already
// existing entries. For example, if there is already an entry of 0x400, 0x200,
// and an fde has a start pc of 0x100 and end pc of 0x500, two new entries
// will be added: 0x200, 0x100 and 0x500, 0x400.
template <typename AddressType>
void DwarfSectionImpl<AddressType>::InsertFde(const DwarfFde* fde) {
  uint64_t start = fde->pc_start;
  uint64_t end = fde->pc_end;
  auto it = fdes_.upper_bound(start);
  while (it != fdes_.end() && start < end && it->second.first < end) {
    if (start < it->second.first) {
      fdes_[it->second.first] = std::make_pair(start, fde);
    }
    start = it->first;
    ++it;
  }
  if (start < end) {
    fdes_[end] = std::make_pair(start, fde);
  }
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::GetNextCieOrFde(const DwarfFde** fde_entry) {
  uint64_t start_offset = next_entries_offset_;

  memory_.set_data_offset(entries_offset_);
  memory_.set_cur_offset(next_entries_offset_);
  uint32_t value32;
  if (!memory_.ReadBytes(&value32, sizeof(value32))) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  uint64_t cie_offset;
  uint8_t cie_fde_encoding;
  bool entry_is_cie = false;
  if (value32 == static_cast<uint32_t>(-1)) {
    // 64 bit entry.
    uint64_t value64;
    if (!memory_.ReadBytes(&value64, sizeof(value64))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }

    next_entries_offset_ = memory_.cur_offset() + value64;
    // Read the Cie Id of a Cie or the pointer of the Fde.
    if (!memory_.ReadBytes(&value64, sizeof(value64))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }

    if (value64 == cie64_value_) {
      entry_is_cie = true;
      cie_fde_encoding = DW_EH_PE_sdata8;
    } else {
      cie_offset = GetCieOffsetFromFde64(value64);
    }
  } else {
    next_entries_offset_ = memory_.cur_offset() + value32;

    // 32 bit Cie
    if (!memory_.ReadBytes(&value32, sizeof(value32))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }

    if (value32 == cie32_value_) {
      entry_is_cie = true;
      cie_fde_encoding = DW_EH_PE_sdata4;
    } else {
      cie_offset = GetCieOffsetFromFde32(value32);
    }
  }

  if (entry_is_cie) {
    auto entry = cie_entries_.find(start_offset);
    if (entry == cie_entries_.end()) {
      DwarfCie* cie = &cie_entries_[start_offset];
      cie->lsda_encoding = DW_EH_PE_omit;
      cie->cfa_instructions_end = next_entries_offset_;
      cie->fde_address_encoding = cie_fde_encoding;

      if (!FillInCie(cie)) {
        cie_entries_.erase(start_offset);
        return false;
      }
    }
    *fde_entry = nullptr;
  } else {
    auto entry = fde_entries_.find(start_offset);
    if (entry != fde_entries_.end()) {
      *fde_entry = &entry->second;
    } else {
      DwarfFde* fde = &fde_entries_[start_offset];
      fde->cfa_instructions_end = next_entries_offset_;
      fde->cie_offset = cie_offset;

      if (!FillInFde(fde)) {
        fde_entries_.erase(start_offset);
        return false;
      }
      *fde_entry = fde;
    }
  }
  return true;
}

template <typename AddressType>
void DwarfSectionImpl<AddressType>::GetFdes(std::vector<const DwarfFde*>* fdes) {
  // Loop through the already cached entries.
  uint64_t entry_offset = entries_offset_;
  while (entry_offset < next_entries_offset_) {
    auto cie_it = cie_entries_.find(entry_offset);
    if (cie_it != cie_entries_.end()) {
      entry_offset = cie_it->second.cfa_instructions_end;
    } else {
      auto fde_it = fde_entries_.find(entry_offset);
      if (fde_it == fde_entries_.end()) {
        // No fde or cie at this entry, should not be possible.
        return;
      }
      entry_offset = fde_it->second.cfa_instructions_end;
      fdes->push_back(&fde_it->second);
    }
  }

  while (next_entries_offset_ < entries_end_) {
    const DwarfFde* fde;
    if (!GetNextCieOrFde(&fde)) {
      break;
    }
    if (fde != nullptr) {
      InsertFde(fde);
      fdes->push_back(fde);
    }

    if (next_entries_offset_ < memory_.cur_offset()) {
      // Simply consider the processing done in this case.
      break;
    }
  }
}

template <typename AddressType>
const DwarfFde* DwarfSectionImpl<AddressType>::GetFdeFromPc(uint64_t pc) {
  // Search in the list of fdes we already have.
  auto it = fdes_.upper_bound(pc);
  if (it != fdes_.end()) {
    if (pc >= it->second.first) {
      return it->second.second;
    }
  }

  // The section might have overlapping pcs in fdes, so it is necessary
  // to do a linear search of the fdes by pc. As fdes are read, a cached
  // search map is created.
  while (next_entries_offset_ < entries_end_) {
    const DwarfFde* fde;
    if (!GetNextCieOrFde(&fde)) {
      return nullptr;
    }
    if (fde != nullptr) {
      InsertFde(fde);
      if (pc >= fde->pc_start && pc < fde->pc_end) {
        return fde;
      }
    }

    if (next_entries_offset_ < memory_.cur_offset()) {
      // Simply consider the processing done in this case.
      break;
    }
  }
  return nullptr;
}

// Explicitly instantiate DwarfSectionImpl
template class DwarfSectionImpl<uint32_t>;
template class DwarfSectionImpl<uint64_t>;

// Explicitly instantiate DwarfDebugFrame
template class DwarfDebugFrame<uint32_t>;
template class DwarfDebugFrame<uint64_t>;

// Explicitly instantiate DwarfEhFrame
template class DwarfEhFrame<uint32_t>;
template class DwarfEhFrame<uint64_t>;

}  // namespace unwindstack
