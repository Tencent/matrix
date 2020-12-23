//
// Created by Carl on 2020-11-11.
//

#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_REVERSE_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_REVERSE_H

#include <cstdint>
#include "BacktraceDefine.h"
#include "QuickenTable.h"

namespace wechat_backtrace {

#define R4_REG_BIT 0
#define R7_REG_BIT 1
#define R10_REG_BIT 2
#define R11_REG_BIT 3
#define SP_REG_BIT 4
#define LR_REG_BIT 5
#define PC_REG_BIT 6

#define CHECK_REG_BIT(Bits, Reg) (Bits & (1 << Reg))
#define SET_REG_BIT(Bits, Reg) (Bits |= (1 << Reg))

class QuickenTableReverse {

public:
    QuickenTableReverse(QutSections* fut_sections, uintptr_t* regs, unwindstack::Memory* memory,
            unwindstack::Memory* process_memory) : regs_(regs), memory_(memory),
            process_memory_(process_memory), fut_sections_(fut_sections) {}
    ~QuickenTableReverse() {};

    bool EvalReverse(size_t entry_offset);

    uptr cfa_ = 0;
    uptr cfa_next_ = 0;

    bool cfa_set_ = false;

    int8_t regs_bits_ = 0;
    int8_t regs_bits_tmp_ = 0;

    uptr dex_pc_ = 0;
    bool pc_set_ = false;

protected:

    bool Decode32Reverse(const uint32_t * instructions, const size_t amount, const size_t start_pos);
    bool Decode64Reverse(const uint64_t * instructions, const size_t amount, const size_t start_pos);
    bool DecodeReverse(const uptr * instructions, const size_t amount, const size_t start_pos);

    uptr * regs_ = nullptr;

    unwindstack::Memory* memory_ = nullptr;
    unwindstack::Memory* process_memory_ = nullptr;
    QutSections* fut_sections_ = nullptr;
};


}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_REVERSE_H
