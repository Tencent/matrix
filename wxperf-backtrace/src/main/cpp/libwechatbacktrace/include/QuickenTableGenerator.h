//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_GENERATOR_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_GENERATOR_H

#include <deque>
#include <memory>

#include "Log.h"
#include "Errors.h"
#include "DwarfEhFrameWithHdrDecoder.h"
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
        QuickenTableGenerator(unwindstack::Memory *memory,
                              unwindstack::Memory *gnu_debug_data_memory,
                              unwindstack::Memory *process_memory)
                : memory_(memory),
                  gnu_debug_data_memory_(gnu_debug_data_memory),
                  process_memory_(process_memory) {}

        ~QuickenTableGenerator() {};

        bool GenerateUltraQUTSections(
                FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info, FrameInfo debug_frame_info,
                FrameInfo gnu_eh_frame_hdr_info, FrameInfo gnu_eh_frame_info,
                FrameInfo gnu_debug_frame_info,
                FrameInfo arm_exidx_info, QutSections *fut_sections);

        QutErrorCode last_error_code;

        const bool log = false;
        const uptr log_addr = 0;
//        bool log = false;
//        uptr log_addr = 0x14e350;

    protected:
        void DecodeDebugFrameEntriesInstr(FrameInfo debug_frame_info,
                                          QutInstructionsOfEntries *entries_instructions,
                                          uint16_t regs_total, bool gnu_debug_data = false);

        void DecodeEhFrameEntriesInstr(FrameInfo eh_frame_hdr_info, FrameInfo eh_frame_info,
                                       QutInstructionsOfEntries *entries_instructions,
                                       uint16_t regs_total, bool gnu_debug_data = false);

        void DecodeExidxEntriesInstr(FrameInfo arm_exidx_info,
                                     QutInstructionsOfEntries *entries_instructions);

        std::shared_ptr<QutInstructionsOfEntries> MergeFrameEntries(
                std::shared_ptr<QutInstructionsOfEntries> to,
                std::shared_ptr<QutInstructionsOfEntries> from);

        bool PackEntriesToFutSections(
                QutInstructionsOfEntries *entries, QutSections *fut_sections);

        bool GetPrel31Addr(unwindstack::Memory *memory_, uint32_t offset, uint32_t *addr);

        unwindstack::Memory *memory_;
        unwindstack::Memory *gnu_debug_data_memory_;
        unwindstack::Memory *process_memory_;
    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_GENERATOR_H
