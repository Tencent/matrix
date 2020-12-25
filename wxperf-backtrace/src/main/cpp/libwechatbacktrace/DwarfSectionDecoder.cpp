//
// Created by Carl on 2020-10-14.
//

#include <DwarfEncoding.h>
#include <include/unwindstack/DwarfStructs.h>
#include <include/unwindstack/MachineArm.h>
#include <include/unwindstack/MachineArm64.h>
#include <map>
#include <deque>
#include <QuickenTableGenerator.h>
#include <QutStatistics.h>
#include "DwarfCfa.h"
#include "DwarfSectionDecoder.h"
#include "DwarfOp.h"

namespace wechat_backtrace {

    using namespace unwindstack;
    using namespace std;

    template<typename AddressType>
    DwarfSectionDecoder<AddressType>::DwarfSectionDecoder(Memory *memory) : memory_(memory) {}

    template<typename AddressType>
    bool
    DwarfSectionDecoder<AddressType>::Init(uint64_t offset, uint64_t size, int64_t section_bias) {
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

    template<typename AddressType>
    const DwarfFde *DwarfSectionDecoder<AddressType>::GetFdeFromPc(uint64_t pc) {
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
            const DwarfFde *fde;
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

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::GetNextCieOrFde(const DwarfFde **fde_entry) {
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
                cie_fde_encoding = unwindstack::DW_EH_PE_sdata8;
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
                DwarfCie *cie = &cie_entries_[start_offset];
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
                DwarfFde *fde = &fde_entries_[start_offset];
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

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::FillInCie(DwarfCie *cie) {
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
                    if (!memory_.ReadEncodedValue<AddressType>(encoding,
                                                               &cie->personality_handler)) {
                        last_error_.code = DWARF_ERROR_MEMORY_INVALID;
                        last_error_.address = memory_.cur_offset();
                        return false;
                    }
                }
                    break;
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

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::FillInFde(DwarfFde *fde) {
        uint64_t cur_offset = memory_.cur_offset();

        const DwarfCie *cie = GetCieFromOffset(fde->cie_offset);
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
        bool valid = memory_.ReadEncodedValue<AddressType>(cie->fde_address_encoding,
                                                           &fde->pc_start);
        fde->pc_start = AdjustPcFromFde(fde->pc_start);

        memory_.set_pc_offset(0);
        if (!valid ||
            !memory_.ReadEncodedValue<AddressType>(cie->fde_address_encoding, &fde->pc_end)) {
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

    template<typename AddressType>
    const DwarfCie *DwarfSectionDecoder<AddressType>::GetCieFromOffset(uint64_t offset) {
        auto cie_entry = cie_entries_.find(offset);
        if (cie_entry != cie_entries_.end()) {
            return &cie_entry->second;
        }
        DwarfCie *cie = &cie_entries_[offset];
        memory_.set_data_offset(entries_offset_);
        memory_.set_cur_offset(offset);
        if (!FillInCieHeader(cie) || !FillInCie(cie)) {
            // Erase the cached entry.
            cie_entries_.erase(offset);
            return nullptr;
        }
        return cie;
    }

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::FillInCieHeader(DwarfCie *cie) {
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


    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::GetCfaLocationInfo(uint64_t pc, const DwarfFde *fde,
                                                              dwarf_loc_regs_t *loc_regs) {
        DwarfCfa<AddressType> cfa(&memory_, fde);

        // Look for the cached copy of the cie data.
        auto reg_entry = cie_loc_regs_.find(fde->cie_offset);
        if (reg_entry == cie_loc_regs_.end()) {
            if (!cfa.GetLocationInfo(pc, fde->cie->cfa_instructions_offset,
                                     fde->cie->cfa_instructions_end,
                                     loc_regs)) {
                last_error_ = cfa.last_error();
                return false;
            }
            cie_loc_regs_[fde->cie_offset] = *loc_regs;
        }
        cfa.set_cie_loc_regs(&cie_loc_regs_[fde->cie_offset]);
        if (!cfa.GetLocationInfo(pc, fde->cfa_instructions_offset, fde->cfa_instructions_end,
                                 loc_regs)) {
            last_error_ = cfa.last_error();
            return false;
        }
        return true;
    }

    template<typename AddressType>
    const DwarfFde *DwarfSectionDecoder<AddressType>::GetFdeFromOffset(uint64_t offset) {
        auto fde_entry = fde_entries_.find(offset);
        if (fde_entry != fde_entries_.end()) {
            return &fde_entry->second;
        }
        DwarfFde *fde = &fde_entries_[offset];
        memory_.set_data_offset(entries_offset_);
        memory_.set_cur_offset(offset);
        if (!FillInFdeHeader(fde) || !FillInFde(fde)) {
            fde_entries_.erase(offset);
            return nullptr;
        }
        return fde;
    }

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::FillInFdeHeader(DwarfFde *fde) {
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

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::EvalExpression(const DwarfLocation &loc,
                                                          Memory *regular_memory,
                                                          uint16_t regs_total,
                                                          ValueExpression<AddressType> *value_expression,
                                                          bool *is_dex_pc) {
        DwarfOp<AddressType> op(&memory_, regular_memory);
        op.set_regs_info(regs_total);
        // Need to evaluate the op data.
        uint64_t end = loc.values[1];
        uint64_t start = end - loc.values[0];
        if (log) {
            QUT_DEBUG_LOG(
                    "DwarfSectionDecoder<AddressType>::EvalExpression, start %llx, end %llx",
                    (ullint_t) start, (ullint_t) end);
        }
        if (!op.Eval(start, end)) {
            if (log) {
                QUT_DEBUG_LOG(
                        "DwarfSectionDecoder<AddressType>::EvalExpression, op.last_error().code %d",
                        (uint32_t) op.last_error().code);
            }
            if ((op.last_error().code == DWARF_ERROR_EXPRESSION_REACH_BREGX ||
                 op.last_error().code == DWARF_ERROR_EXPRESSION_REACH_BREG) && op.dex_pc_set()) {
                // XXX Maybe we'll support more breg/bregx cases in the future.
                value_expression->reg_expression = op.reg_expression();
            } else {
                last_error_ = op.last_error();
                return false;
            }
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

        value_expression->value = op.StackAt(0);
        if (is_dex_pc != nullptr && op.dex_pc_set()) {
            *is_dex_pc = true;
        }
        return true;
    }

    template<typename AddressType>
    struct EvalInfo {
        const dwarf_loc_regs_t *loc_regs;
        const DwarfCie *cie;
        Memory *regular_memory;
        AddressType cfa;
    };

    template<typename AddressType>
    bool
    DwarfSectionDecoder<AddressType>::EvalRegister(const DwarfLocation *loc, uint16_t regs_total,
                                                   uint32_t reg, void *info) {
        EvalInfo<AddressType> *eval_info = reinterpret_cast<EvalInfo<AddressType> *>(info);
        Memory *regular_memory = eval_info->regular_memory;

        if (log) {
            QUT_DEBUG_LOG("DwarfSectionDecoder::EvalRegister loc->type %u, reg(%u),",
                          (uint32_t) loc->type, reg);
        }
        switch (loc->type) {
            case DWARF_LOCATION_OFFSET: {
                RegOffsetInstruction(reg, loc->values[0]);
                break;
            }
            case DWARF_LOCATION_VAL_OFFSET: {     // Do not support reg = cfa + offset, no QUT instruction.
                if (log) {
                    QUT_DEBUG_LOG("DwarfSectionDecoder::EvalRegister DWARF_LOCATION_VAL_OFFSET");
                }
                last_error_.code = DWARF_ERROR_NOT_SUPPORT;
                QUT_STATISTIC(UnsupportedDwarfLocationValOffset, reg, DWARF_ERROR_NOT_SUPPORT);
                return false;
            }
            case DWARF_LOCATION_REGISTER: {     // Do not support regM = regN + offset, no QUT instruction.
                if (log) {
                    QUT_DEBUG_LOG("DwarfSectionDecoder::EvalRegister DWARF_LOCATION_REGISTER");
                }
                last_error_.code = DWARF_ERROR_NOT_SUPPORT;
                QUT_STATISTIC(UnsupportedDwarfLocationRegister, reg, DWARF_ERROR_NOT_SUPPORT);
                return false;
            }
            case DWARF_LOCATION_EXPRESSION:
            case DWARF_LOCATION_VAL_EXPRESSION: {
                if (log) {
                    QUT_DEBUG_LOG(
                            "DwarfSectionDecoder::EvalRegister DWARF_LOCATION_EXPRESSION DWARF_LOCATION_VAL_EXPRESSION");
                }
                ValueExpression<AddressType> value_expression;
                bool is_dex_pc = false;
                if (!EvalExpression(*loc, regular_memory, regs_total, &value_expression,
                                    &is_dex_pc)) {
                    return false;
                }
                if (loc->type == DWARF_LOCATION_EXPRESSION) {
                    if (log) {
                        QUT_DEBUG_LOG(
                                "DwarfSectionDecoder::EvalRegister DWARF_LOCATION_EXPRESSION");
                    }
                    last_error_.code = DWARF_ERROR_NOT_SUPPORT;
                    QUT_STATISTIC(UnsupportedDwarfLocationExpression, reg, DWARF_ERROR_NOT_SUPPORT);
                    return false;
                } else {
                    if (log) {
                        QUT_DEBUG_LOG(
                                "DwarfSectionDecoder::EvalRegister DWARF_LOCATION_VAL_EXPRESSION is_dex_pc: %d",
                                is_dex_pc);
                    }
                    if (is_dex_pc) {
                        AddressType value = value_expression.value;
                        temp_instructions_->push_back(
                                (QUT_INSTRUCTION_DEX_PC_SET << 32) | (0xffffffff & value));
                    } else {
                        last_error_.code = DWARF_ERROR_NOT_SUPPORT;
                        QUT_STATISTIC(UnsupportedDwarfLocationValExpressionWithoutDexPc, reg,
                                      DWARF_ERROR_NOT_SUPPORT);
                        return false;
                    }
                }
                break;
            }
            case DWARF_LOCATION_UNDEFINED:
                if (log) {
                    QUT_DEBUG_LOG(
                            "DwarfSectionDecoder::EvalRegister DWARF_LOCATION_UNDEFINED");
                }
                if (reg == eval_info->cie->return_address_register) {
                    if (log) {
                        QUT_DEBUG_LOG(
                                "DwarfSectionDecoder::EvalRegister reg == eval_info->cie->return_address_register");
                    }
                    temp_instructions_->push_back((QUT_FIN << 32));
                } else {
                    last_error_.code = DWARF_ERROR_NOT_SUPPORT;
                    QUT_STATISTIC(UnsupportedDwarfLocationUndefined, reg, DWARF_ERROR_NOT_SUPPORT);
                    return false;
                }
                break;
            default:
                QUT_STATISTIC(UnsupportedDwarfLocation, reg, DWARF_ERROR_NOT_SUPPORT);
                break;
        }

        return true;
    }

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::CfaOffsetInstruction(uint64_t reg, uint64_t value) {

        QutInstruction instruction;
        switch (reg) {
#ifdef __arm__
            case ARM_REG_SP:
                instruction = QUT_INSTRUCTION_VSP_OFFSET;
                break;
            case ARM_REG_R10:
                instruction = QUT_INSTRUCTION_VSP_SET_BY_JNI_SP;
                break;
            case ARM_REG_R7:
                instruction = QUT_INSTRUCTION_VSP_SET_BY_R7;
                break;
            case ARM_REG_R11:
                instruction = QUT_INSTRUCTION_VSP_SET_BY_R11;
                break;
#else
            case ARM64_REG_SP:
                instruction = QUT_INSTRUCTION_VSP_OFFSET;
                break;
            case ARM64_REG_R28:
                instruction = QUT_INSTRUCTION_VSP_SET_BY_JNI_SP;
                break;
            case ARM64_REG_R29:
                instruction = QUT_INSTRUCTION_VSP_SET_BY_X29;
                break;
#endif
            default:
                QUT_STATISTIC(UnsupportedCfaDwarfLocationRegister, reg, value);
                return false;
        }

        // TODO check value overflow
        if (log) {
            QUT_DEBUG_LOG(
                    "DwarfSectionDecoder<AddressType>::CfaOffsetInstruction reg(%u) %llu",
                    (uint32_t) reg, (ullint_t) value);
        }
        temp_instructions_->push_back((instruction << 32) | (0xffffffff & value));

        return true;
    }

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::RegOffsetInstruction(uint64_t reg, uint64_t value) {

        QutInstruction instruction;
        switch (reg) {
#ifdef __arm__
            case ARM_REG_R4:
                instruction = QUT_INSTRUCTION_R4_OFFSET;
                break;
            case ARM_REG_R7:
                instruction = QUT_INSTRUCTION_R7_OFFSET;
                break;
            case ARM_REG_R10:
                instruction = QUT_INSTRUCTION_R10_OFFSET;
                break;
            case ARM_REG_R11:
                instruction = QUT_INSTRUCTION_R11_OFFSET;
                break;
            case ARM_REG_SP:
                instruction = QUT_INSTRUCTION_SP_OFFSET;
                break;
            case ARM_REG_LR:
                instruction = QUT_INSTRUCTION_LR_OFFSET;
                break;
            case ARM_REG_PC:
                instruction = QUT_INSTRUCTION_PC_OFFSET;
                break;
#else
            case ARM64_REG_R20:
                instruction = QUT_INSTRUCTION_X20_OFFSET;
                break;
            case ARM64_REG_R28:
                instruction = QUT_INSTRUCTION_X28_OFFSET;
                break;
            case ARM64_REG_R29:
                instruction = QUT_INSTRUCTION_X29_OFFSET;
                break;
            case ARM64_REG_SP:
                instruction = QUT_INSTRUCTION_SP_OFFSET;
                break;
            case ARM64_REG_LR:
                instruction = QUT_INSTRUCTION_LR_OFFSET;
                break;
            case ARM64_REG_PC:
                instruction = QUT_INSTRUCTION_PC_OFFSET;
                break;
#endif
            default:
                QUT_STATISTIC(UnsupportedDwarfLocationOffset, reg, value);
                return false;
        }

        if (log) {
            QUT_DEBUG_LOG(
                    "DwarfSectionDecoder::RegOffsetInstruction instruction: %llu, value: %lld",
                    (ullint_t) instruction, (llint_t) value);
        }

        // TODO check value overflow
        temp_instructions_->push_back((instruction << 32) | (0xffffffff & ((int32_t) -value)));

        return true;
    }

    template<typename AddressType>
    bool DwarfSectionDecoder<AddressType>::Eval(const DwarfCie *cie, Memory *regular_memory,
                                                const dwarf_loc_regs_t &loc_regs,
                                                uint16_t total_regs) {
        if (log) {
            QUT_DEBUG_LOG("DwarfSectionImpl<AddressType>::Eval %llx",
                          (ullint_t) cie->cfa_instructions_offset);
        }

        if (cie->return_address_register >= total_regs) {
            last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
            if (log) {
                QUT_DEBUG_LOG("DwarfSectionImpl::Eval DWARF_ERROR_ILLEGAL_VALUE");
            }
            return false;
        }

        // Get the cfa value;
        auto cfa_entry = loc_regs.find(CFA_REG);
        if (cfa_entry == loc_regs.end()) {
            last_error_.code = DWARF_ERROR_CFA_NOT_DEFINED;
            if (log) {
                QUT_DEBUG_LOG("DwarfSectionImpl::Eval DWARF_ERROR_CFA_NOT_DEFINED");
            }
            return false;
        }

        EvalInfo<AddressType> eval_info{.loc_regs = &loc_regs,
                .cie = cie,
                .regular_memory = regular_memory,
        };

        const DwarfLocation *loc = &cfa_entry->second;
        // Only a few location types are valid for the cfa.

        switch (loc->type) {
            case DWARF_LOCATION_REGISTER:
                if (log) {
                    QUT_DEBUG_LOG("DwarfSectionImpl::Eval DWARF_LOCATION_REGISTER");
                }
                if (loc->values[0] >= total_regs) {
                    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
                    if (log) {
                        QUT_DEBUG_LOG(
                                "QUT_DEBUG_LOG::Eval DWARF_LOCATION_REGISTER DWARF_ERROR_ILLEGAL_VALUE");
                    }
                    return false;
                }
//            eval_info.cfa = (*cur_regs)[];
//            eval_info.cfa += loc->values[1];

                if (!CfaOffsetInstruction(loc->values[0], loc->values[1])) {
                    last_error_.code = DWARF_ERROR_NOT_SUPPORT;
                    if (log) {
                        QUT_DEBUG_LOG(
                                "DwarfSectionImpl::Eval DWARF_LOCATION_REGISTER DWARF_ERROR_NOT_SUPPORT");
                    }
                    return false;
                }

                if (log) {
                    QUT_DEBUG_LOG(
                            "DWARF_LOCATION_REGISTER eval_info.cfa: 0x%" "x" " = reg(%u) + %x",
                            (uint32_t) eval_info.cfa, (int32_t) loc->values[0],
                            (int32_t) loc->values[1]);
                }
                break;
            case DWARF_LOCATION_VAL_EXPRESSION: {
                if (log) {
                    QUT_DEBUG_LOG(
                            "DwarfSectionDecoder::Eval DWARF_LOCATION_VAL_EXPRESSION");
                }
                ValueExpression<AddressType> value_expression;
                if (!EvalExpression(*loc, regular_memory, total_regs, &value_expression, nullptr)) {
                    return false;
                }
                AddressType value = value_expression.value;
                // TODO check value overflow
                temp_instructions_->push_back(
                        (QUT_INSTRUCTION_VSP_SET_IMM << 32) | (0xffffffff & value));
                QUT_STATISTIC(UnsupportedCfaDwarfLocationValExpression, value, 0);
                break;
            }
            default:
                last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
                if (log) {
                    QUT_DEBUG_LOG("DwarfSectionImpl::Eval DWARF_ERROR_ILLEGAL_VALUE");
                }
                return false;
        }

        for (const auto &entry : loc_regs) {
            uint32_t reg = entry.first;
            // Already handled the CFA register.
            if (reg == CFA_REG) continue;

            if (reg >= total_regs) {
                // Skip this unknown register.
                continue;
            }

#ifdef __arm__
            // TODO why ARM_REG_R0, add comment here
            // Why evaluate r0:
            // Why evaluate r4:
            // Why evaluate r7:
            // Why evaluate r10:
            // Why evaluate r11:
            if (reg != ARM_REG_R0 && reg != ARM_REG_R4 && reg != ARM_REG_R7 && reg != ARM_REG_R10 &&
                reg != ARM_REG_R11 && reg < ARM_REG_R13) {
                continue;
            }
#else
            // Why evaluate x0:
            // Why evaluate x20:
            // Why evaluate x28:
            if (reg != ARM64_REG_R0 &&reg != ARM64_REG_R20 && reg != ARM64_REG_R28 && reg < ARM64_REG_R29) {
                continue;
            }
#endif

            if (!EvalRegister(&entry.second, total_regs, reg, &eval_info)) {
                if (log) {
                    QUT_DEBUG_LOG("DwarfSectionDecoder::Eval EvalRegister false");
                }
                return false;
            }
        }

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
    template<typename AddressType>
    void DwarfSectionDecoder<AddressType>::InsertFde(const DwarfFde *fde) {
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

    template<typename AddressType>
    void
    DwarfSectionDecoder<AddressType>::IterateAllEntries(uint16_t regs_total,
                                                        unwindstack::Memory *process_memory,
                                                        QutInstructionsOfEntries *previous_entries) {

        FillFdes();

        QutInstructionsOfEntries *all_instructions = previous_entries;

        map<uint64_t, pair<uint64_t, const DwarfFde *>>::iterator it;
        it = fdes_.begin();

        last_error_.code = DWARF_ERROR_NONE;

        QUT_DEBUG_LOG("DwarfSectionDecoder::IterateAllEntries fdes_ size %zu", fdes_.size());

        while (it != fdes_.end()) {
            const DwarfFde *fde = it->second.second;

            it++;

            if (fde == nullptr || fde->cie == nullptr) {
                // TODO bad entry
                continue;
            }

            deque<uint64_t> *prev_instructions = nullptr;
            uint64_t prev_pc = -1;
            for (uint64_t pc = fde->pc_start; pc < fde->pc_end;) {

                // Now get the location information for this pc.
                dwarf_loc_regs_t loc_regs;
                if (!GetCfaLocationInfo(pc, fde, &loc_regs)) {
                    // TODO bad entry
                    prev_instructions = nullptr;
                    prev_pc = -1;
                    pc += 2;
                    continue;
                }
                loc_regs.cie = fde->cie;

                uint64_t pc_end = loc_regs.pc_end;

//                log = log_pc >= pc && log_pc < pc_end;

                auto instructions = make_shared<deque<uint64_t>>();

                temp_instructions_ = instructions;

                Eval(loc_regs.cie, process_memory, loc_regs, regs_total);

                temp_instructions_ = nullptr;

                if (log) {
                    QUT_DEBUG_LOG("Evaluated instructions size: %zu", instructions->size());
                }

                // Try merge same entries.
                bool same_entry = false;
                if (prev_instructions != nullptr &&
                    prev_instructions->size() == instructions.get()->size()) {

                    same_entry = true;
                    for (size_t i = 0; i < instructions.get()->size(); i++) {
                        if (instructions.get()->at(i) != prev_instructions->at(i)) {
                            same_entry = false;
                            break;
                        }
                    }
                }

                if (log) {
                    QUT_DEBUG_LOG("Evaluated same_entry: %d", same_entry);
                    QUT_DEBUG_LOG("Evaluated pc: %llx", (ullint_t) pc);
                }

                if (same_entry) {
                    (*all_instructions)[prev_pc].first = pc_end;
                    pc = pc_end;
                    continue;
                }

                (*all_instructions)[pc] = std::make_pair(pc_end, instructions);

                prev_instructions = instructions.get();
                prev_pc = pc;

                pc = pc_end;
            }
        }


    }

    template<typename AddressType>
    void DwarfSectionDecoder<AddressType>::FillFdes() {
        // The section might have overlapping pcs in fdes, so it is necessary
        // to do a linear search of the fdes by pc. As fdes are read, a cached
        // search map is created.
        while (next_entries_offset_ < entries_end_) {
            const DwarfFde *fde;
            if (!GetNextCieOrFde(&fde)) {
                continue;
            }
            if (fde != nullptr) {
                InsertFde(fde);
            }

            if (next_entries_offset_ < memory_.cur_offset()) {
                // Simply consider the processing done in this case.
                break;
            }
        }
    }

    // Explicitly instantiate DwarfSectionDecoder
    template
    class DwarfSectionDecoder<uint32_t>;

    template
    class DwarfSectionDecoder<uint64_t>;

}  // namespace wechat_backtrace