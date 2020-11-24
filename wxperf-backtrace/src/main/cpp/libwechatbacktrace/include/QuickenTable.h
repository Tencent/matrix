//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H

#include <cstdint>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "unwindstack/Elf.h"
#include "unwindstack/Memory.h"
#include "Predefined.h"
#include "Errors.h"

namespace wechat_backtrace {

    struct QutSections {

        QutSections() = default;

        ~QutSections() {
            if (!load_from_file) {
                delete quidx;
                delete qutbl;
            }

            idx_size = 0;
            tbl_size = 0;

            idx_capacity = 0;
            tbl_capacity = 0;
        }

        uptr *quidx = nullptr;
        uptr *qutbl = nullptr;

        size_t idx_size = 0;
        size_t tbl_size = 0;

        size_t idx_capacity = 0;
        size_t tbl_capacity = 0;

//        size_t total_entries = 0;
//        size_t start_offset_ = 0;

        bool load_from_file = false;
    };

    typedef QutSections *QutSectionsPtr;

    class QuickenTable {

    public:
        QuickenTable(QutSections *fut_sections, uintptr_t *regs, unwindstack::Memory *memory,
                     unwindstack::Memory *process_memory, uptr stack_top, uptr stack_bottom)
                : regs_(regs), memory_(memory), process_memory_(process_memory),
                  fut_sections_(fut_sections), stack_top_(stack_top),
                  stack_bottom_(stack_bottom) {};

        ~QuickenTable() {};

        QutErrorCode Eval(size_t entry_offset);

        uptr cfa_ = 0;
        uptr dex_pc_ = 0;
        bool pc_set_ = false;

    protected:

        QutErrorCode Decode32(const uint32_t *instructions, const size_t amount, const size_t start_pos);

        QutErrorCode Decode64(const uint64_t *instructions, const size_t amount, const size_t start_pos);

        QutErrorCode Decode(const uptr *instructions, const size_t amount, const size_t start_pos);

        bool ReadStack(const uptr addr, uptr *value);

        uptr *regs_ = nullptr;

        unwindstack::Memory *memory_ = nullptr;
        unwindstack::Memory *process_memory_ = nullptr;
        QutSections *fut_sections_ = nullptr;

        const uptr stack_top_;
        const uptr stack_bottom_;
    };


}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H
