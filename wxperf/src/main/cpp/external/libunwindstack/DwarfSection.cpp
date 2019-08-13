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

const DwarfFde* DwarfSection::GetFdeFromPc(uint64_t pc) {
  uint64_t fde_offset;
  if (!GetFdeOffsetFromPc(pc, &fde_offset)) {
    return nullptr;
  }
  const DwarfFde* fde = GetFdeFromOffset(fde_offset);
  if (fde == nullptr) {
    return nullptr;
  }

  // Guaranteed pc >= pc_start, need to check pc in the fde range.
  if (pc < fde->pc_end) {
    return fde;
  }
  last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
  return nullptr;
}

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
const DwarfCie* DwarfSectionImpl<AddressType>::GetCie(uint64_t offset) {
  auto cie_entry = cie_entries_.find(offset);
  if (cie_entry != cie_entries_.end()) {
    return &cie_entry->second;
  }
  DwarfCie* cie = &cie_entries_[offset];
  memory_.set_cur_offset(offset);
  if (!FillInCie(cie)) {
    // Erase the cached entry.
    cie_entries_.erase(offset);
    return nullptr;
  }
  return cie;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::FillInCie(DwarfCie* cie) {
  uint32_t length32;
  if (!memory_.ReadBytes(&length32, sizeof(length32))) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }
  // Set the default for the lsda encoding.
  cie->lsda_encoding = DW_EH_PE_omit;

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

  if (!memory_.ReadBytes(&cie->version, sizeof(cie->version))) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  if (cie->version != 1 && cie->version != 3 && cie->version != 4) {
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

  if (cie->version == 4) {
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
  memory_.set_cur_offset(offset);
  if (!FillInFde(fde)) {
    fde_entries_.erase(offset);
    return nullptr;
  }
  return fde;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::FillInFde(DwarfFde* fde) {
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
  uint64_t cur_offset = memory_.cur_offset();

  const DwarfCie* cie = GetCie(fde->cie_offset);
  if (cie == nullptr) {
    return false;
  }
  fde->cie = cie;

  if (cie->segment_size != 0) {
    // Skip over the segment selector for now.
    cur_offset += cie->segment_size;
  }
  memory_.set_cur_offset(cur_offset);

  if (!memory_.ReadEncodedValue<AddressType>(cie->fde_address_encoding & 0xf, &fde->pc_start)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }
  fde->pc_start = AdjustPcFromFde(fde->pc_start);

  if (!memory_.ReadEncodedValue<AddressType>(cie->fde_address_encoding & 0xf, &fde->pc_end)) {
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
bool DwarfSectionImpl<AddressType>::Log(uint8_t indent, uint64_t pc, uint64_t load_bias,
                                        const DwarfFde* fde) {
  DwarfCfa<AddressType> cfa(&memory_, fde);

  // Always print the cie information.
  const DwarfCie* cie = fde->cie;
  if (!cfa.Log(indent, pc, load_bias, cie->cfa_instructions_offset, cie->cfa_instructions_end)) {
    last_error_ = cfa.last_error();
    return false;
  }
  if (!cfa.Log(indent, pc, load_bias, fde->cfa_instructions_offset, fde->cfa_instructions_end)) {
    last_error_ = cfa.last_error();
    return false;
  }
  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::Init(uint64_t offset, uint64_t size) {
  entries_offset_ = offset;
  entries_end_ = offset + size;

  memory_.clear_func_offset();
  memory_.clear_text_offset();
  memory_.set_data_offset(offset);
  memory_.set_cur_offset(offset);
  memory_.set_pc_offset(offset);

  return CreateSortedFdeList();
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::GetCieInfo(uint8_t* segment_size, uint8_t* encoding) {
  uint8_t version;
  if (!memory_.ReadBytes(&version, 1)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }
  // Read the augmentation string.
  std::vector<char> aug_string;
  char aug_value;
  bool get_encoding = false;
  do {
    if (!memory_.ReadBytes(&aug_value, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
    if (aug_value == 'R') {
      get_encoding = true;
    }
    aug_string.push_back(aug_value);
  } while (aug_value != '\0');

  if (version == 4) {
    // Skip the Address Size field.
    memory_.set_cur_offset(memory_.cur_offset() + 1);

    // Read the segment size.
    if (!memory_.ReadBytes(segment_size, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
  } else {
    *segment_size = 0;
  }

  if (aug_string[0] != 'z' || !get_encoding) {
    // No encoding
    return true;
  }

  // Skip code alignment factor
  uint8_t value;
  do {
    if (!memory_.ReadBytes(&value, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
  } while (value & 0x80);

  // Skip data alignment factor
  do {
    if (!memory_.ReadBytes(&value, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
  } while (value & 0x80);

  if (version == 1) {
    // Skip return address register.
    memory_.set_cur_offset(memory_.cur_offset() + 1);
  } else {
    // Skip return address register.
    do {
      if (!memory_.ReadBytes(&value, 1)) {
        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
        last_error_.address = memory_.cur_offset();
        return false;
      }
    } while (value & 0x80);
  }

  // Skip the augmentation length.
  do {
    if (!memory_.ReadBytes(&value, 1)) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }
  } while (value & 0x80);

  for (size_t i = 1; i < aug_string.size(); i++) {
    if (aug_string[i] == 'R') {
      if (!memory_.ReadBytes(encoding, 1)) {
        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
        last_error_.address = memory_.cur_offset();
        return false;
      }
      // Got the encoding, that's all we are looking for.
      return true;
    } else if (aug_string[i] == 'L') {
      memory_.set_cur_offset(memory_.cur_offset() + 1);
    } else if (aug_string[i] == 'P') {
      uint8_t encoding;
      if (!memory_.ReadBytes(&encoding, 1)) {
        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
        last_error_.address = memory_.cur_offset();
        return false;
      }
      uint64_t value;
      if (!memory_.template ReadEncodedValue<AddressType>(encoding, &value)) {
        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
        last_error_.address = memory_.cur_offset();
        return false;
      }
    }
  }

  // It should be impossible to get here.
  abort();
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::AddFdeInfo(uint64_t entry_offset, uint8_t segment_size,
                                               uint8_t encoding) {
  if (segment_size != 0) {
    memory_.set_cur_offset(memory_.cur_offset() + 1);
  }

  uint64_t start;
  if (!memory_.template ReadEncodedValue<AddressType>(encoding & 0xf, &start)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }
  start = AdjustPcFromFde(start);

  uint64_t length;
  if (!memory_.template ReadEncodedValue<AddressType>(encoding & 0xf, &length)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }
  if (length != 0) {
    fdes_.emplace_back(entry_offset, start, length);
  }

  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::CreateSortedFdeList() {
  memory_.set_cur_offset(entries_offset_);

  // Loop through all of the entries and read just enough to create
  // a sorted list of pcs.
  // This code assumes that first comes the cie, then the fdes that
  // it applies to.
  uint64_t cie_offset = 0;
  uint8_t address_encoding;
  uint8_t segment_size;
  while (memory_.cur_offset() < entries_end_) {
    uint64_t cur_entry_offset = memory_.cur_offset();

    // Figure out the entry length and type.
    uint32_t value32;
    if (!memory_.ReadBytes(&value32, sizeof(value32))) {
      last_error_.code = DWARF_ERROR_MEMORY_INVALID;
      last_error_.address = memory_.cur_offset();
      return false;
    }

    uint64_t next_entry_offset;
    if (value32 == static_cast<uint32_t>(-1)) {
      uint64_t value64;
      if (!memory_.ReadBytes(&value64, sizeof(value64))) {
        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
        last_error_.address = memory_.cur_offset();
        return false;
      }
      next_entry_offset = memory_.cur_offset() + value64;

      // Read the Cie Id of a Cie or the pointer of the Fde.
      if (!memory_.ReadBytes(&value64, sizeof(value64))) {
        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
        last_error_.address = memory_.cur_offset();
        return false;
      }

      if (value64 == cie64_value_) {
        // Cie 64 bit
        address_encoding = DW_EH_PE_sdata8;
        if (!GetCieInfo(&segment_size, &address_encoding)) {
          return false;
        }
        cie_offset = cur_entry_offset;
      } else {
        uint64_t last_cie_offset = GetCieOffsetFromFde64(value64);
        if (last_cie_offset != cie_offset) {
          // This means that this Fde is not following the Cie.
          last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
          return false;
        }

        // Fde 64 bit
        if (!AddFdeInfo(cur_entry_offset, segment_size, address_encoding)) {
          return false;
        }
      }
    } else {
      next_entry_offset = memory_.cur_offset() + value32;

      // Read the Cie Id of a Cie or the pointer of the Fde.
      if (!memory_.ReadBytes(&value32, sizeof(value32))) {
        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
        last_error_.address = memory_.cur_offset();
        return false;
      }

      if (value32 == cie32_value_) {
        // Cie 32 bit
        address_encoding = DW_EH_PE_sdata4;
        if (!GetCieInfo(&segment_size, &address_encoding)) {
          return false;
        }
        cie_offset = cur_entry_offset;
      } else {
        uint64_t last_cie_offset = GetCieOffsetFromFde32(value32);
        if (last_cie_offset != cie_offset) {
          // This means that this Fde is not following the Cie.
          last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
          return false;
        }

        // Fde 32 bit
        if (!AddFdeInfo(cur_entry_offset, segment_size, address_encoding)) {
          return false;
        }
      }
    }

    if (next_entry_offset < memory_.cur_offset()) {
      // Simply consider the processing done in this case.
      break;
    }
    memory_.set_cur_offset(next_entry_offset);
  }

  // Sort the entries.
  std::sort(fdes_.begin(), fdes_.end(), [](const FdeInfo& a, const FdeInfo& b) {
    if (a.start == b.start) return a.end < b.end;
    return a.start < b.start;
  });

  fde_count_ = fdes_.size();

  return true;
}

template <typename AddressType>
bool DwarfSectionImpl<AddressType>::GetFdeOffsetFromPc(uint64_t pc, uint64_t* fde_offset) {
  if (fde_count_ == 0) {
    return false;
  }

  size_t first = 0;
  size_t last = fde_count_;
  while (first < last) {
    size_t current = (first + last) / 2;
    const FdeInfo* info = &fdes_[current];
    if (pc >= info->start && pc <= info->end) {
      *fde_offset = info->offset;
      return true;
    }

    if (pc < info->start) {
      last = current;
    } else {
      first = current + 1;
    }
  }
  return false;
}

template <typename AddressType>
const DwarfFde* DwarfSectionImpl<AddressType>::GetFdeFromIndex(size_t index) {
  if (index >= fdes_.size()) {
    return nullptr;
  }
  return this->GetFdeFromOffset(fdes_[index].offset);
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
