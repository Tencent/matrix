//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_GENERATOR_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_GENERATOR_H

#include <deque>
#include <memory>

#include "../../common/Log.h"
#include "../../libunwindstack/ArmExidx.h"
#include "Errors.h"
#include "../DwarfEhFrameWithHdrDecoder.h"
#include "MinimalRegs.h"
#include "QuickenInstructions.h"
#include "QuickenTable.h"

namespace wechat_backtrace {

    struct FrameInfo {
        uint64_t offset_ = 0;
        int64_t section_bias_ = 0;
        uint64_t size_ = 0;
    };

    template<typename AddressType>
    class QuickenTableGenerator {

    public:
        QuickenTableGenerator(unwindstack::Memory *memory, unwindstack::Memory *process_memory)
                : memory_(memory), process_memory_(process_memory) {}

        ~QuickenTableGenerator() {};

        bool GenerateUltraQUTSections(
                FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info, FrameInfo debug_frame_info,
                FrameInfo gnu_eh_frame_hdr_info, FrameInfo gnu_eh_frame_info,
                FrameInfo gnu_debug_frame_info,
                FrameInfo arm_exidx_info, QutSections *fut_sections);

        QutErrorCode last_error_code;

        static std::shared_ptr<QutSections> FindQutSections(std::string sopath);

    protected:
        void DecodeDebugFrameEntriesInstr(FrameInfo debug_frame_info,
                                          QutInstructionsOfEntries *entries_instructions,
                                          uint16_t regs_total);

        void DecodeEhFrameEntriesInstr(FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info,
                                       QutInstructionsOfEntries *entries_instructions,
                                       uint16_t regs_total);

        void DecodeExidxEntriesInstr(FrameInfo arm_exidx_info,
                                     QutInstructionsOfEntries *entries_instructions);

        std::shared_ptr<QutInstructionsOfEntries> MergeFrameEntries(
                std::shared_ptr<QutInstructionsOfEntries> to,
                std::shared_ptr<QutInstructionsOfEntries> from);

        bool PackEntriesToFutSections(
                QutInstructionsOfEntries *entries, QutSections *fut_sections);

        bool GetPrel31Addr(uint32_t offset, uint32_t *addr);

        unwindstack::Memory *memory_;
        unwindstack::Memory *process_memory_;
    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_GENERATOR_H
