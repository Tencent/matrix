//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBUNWINDSTACK_WECHAT_QUICKEN_DWARF_SECTION_DECODER_H
#define _LIBUNWINDSTACK_WECHAT_QUICKEN_DWARF_SECTION_DECODER_H

#include <deque>
#include <map>
#include <unwindstack/Memory.h>
#include <unwindstack/DwarfStructs.h>
#include <unwindstack/DwarfLocation.h>
#include <unwindstack/DwarfMemory.h>
#include <unwindstack/DwarfError.h>
#include <unwindstack/Regs.h>

#include "Log.h"
#include "Errors.h"

namespace wechat_backtrace {

    template<typename AddressType>
    struct ValueExpression {
        AddressType value;
        uint16_t reg_expression;
    };

    typedef std::vector<uint64_t> QutInstrCollection;
    typedef std::map<uint64_t, std::pair<uint64_t, std::shared_ptr<QutInstrCollection>>> QutInstructionsOfEntries;

    template<typename AddressType>
    class DwarfSectionDecoder {

    public:
        DwarfSectionDecoder(unwindstack::Memory *memory);

        virtual ~DwarfSectionDecoder() = default;

        virtual bool Init(uint64_t offset, uint64_t size, int64_t section_bias);

        virtual const unwindstack::DwarfFde *GetFdeFromPc(uint64_t pc);

        virtual bool GetCfaLocationInfo(uint64_t pc, const unwindstack::DwarfFde *fde,
                                        unwindstack::dwarf_loc_regs_t *loc_regs);

        virtual uint64_t GetCieOffsetFromFde32(uint32_t pointer) = 0;

        virtual uint64_t GetCieOffsetFromFde64(uint64_t pointer) = 0;

        virtual uint64_t AdjustPcFromFde(uint64_t pc) = 0;

        void IterateAllEntries(uint16_t regs_total, unwindstack::Memory *process_memory,
                               QutInstructionsOfEntries *, uint64_t &estimate_memory_usage,
                               bool &memory_overwhelmed);

        bool Eval(const unwindstack::DwarfCie *, unwindstack::Memory *,
                  const unwindstack::dwarf_loc_regs_t &, uint16_t total_regs);

        const unwindstack::DwarfCie *GetCieFromOffset(uint64_t offset);

        const unwindstack::DwarfFde *GetFdeFromOffset(uint64_t offset);

        DwarfErrorCode LastErrorCode() { return last_error_.code; }

        uint64_t LastErrorAddress() { return last_error_.address; }

    protected:

        void FillFdes();

        bool GetNextCieOrFde(const unwindstack::DwarfFde **fde_entry);

        unwindstack::DwarfMemory memory_;

        uint32_t cie32_value_ = 0;
        uint64_t cie64_value_ = 0;

        std::unordered_map<uint64_t, unwindstack::DwarfFde> fde_entries_;
        std::unordered_map<uint64_t, unwindstack::DwarfCie> cie_entries_;
        std::unordered_map<uint64_t, unwindstack::dwarf_loc_regs_t> cie_loc_regs_;
        std::map<uint64_t, unwindstack::dwarf_loc_regs_t> loc_regs_;  // Single row indexed by pc_end.

        bool EvalRegister(const unwindstack::DwarfLocation *loc, uint16_t regs_total, uint32_t reg,
                          void *info);

        bool
        EvalExpression(const unwindstack::DwarfLocation &loc, unwindstack::Memory *regular_memory,
                       uint16_t regs_total, ValueExpression<AddressType> *value_expression,
                       bool *is_dex_pc);

        bool FillInCieHeader(unwindstack::DwarfCie *cie);

        bool FillInCie(unwindstack::DwarfCie *cie);

        bool FillInFdeHeader(unwindstack::DwarfFde *fde);

        bool FillInFde(unwindstack::DwarfFde *fde);

        void InsertFde(const unwindstack::DwarfFde *fde);

        bool CfaOffsetInstruction(uint64_t reg, uint64_t value);

        bool RegOffsetInstruction(uint64_t reg, uint64_t value);

        int64_t section_bias_ = 0;
        uint64_t entries_offset_ = 0;
        uint64_t entries_end_ = 0;
        uint64_t next_entries_offset_ = 0;
        uint64_t pc_offset_ = 0;

        std::map<uint64_t, std::pair<uint64_t, const unwindstack::DwarfFde *>> fdes_;

        DwarfErrorData last_error_{DWARF_ERROR_NONE, 0};

        const bool log = false;
        const uptr log_pc = 0;
//        bool log = true;
//        uptr log_pc = 0x13f994;

        std::shared_ptr<QutInstrCollection> temp_instructions_;
    };

}  // namespace wechat_backtrace

#endif  // _LIBUNWINDSTACK_WECHAT_QUICKEN_DWARF_SECTION_DECODER_H
