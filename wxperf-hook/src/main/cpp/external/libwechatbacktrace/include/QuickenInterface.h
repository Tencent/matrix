#ifndef _LIBWECHATBACKTRACE_QUICKEN_INTERFACE_H
#define _LIBWECHATBACKTRACE_QUICKEN_INTERFACE_H

#include <stdint.h>
#include <memory>
#include <mutex>

#include <unwindstack/Memory.h>
#include <unwindstack/Error.h>
#include "QuickenTableGenerator.h"
#include "Errors.h"

namespace wechat_backtrace {

class QuickenInterface {

public:
    QuickenInterface(unwindstack::Memory* memory, uint64_t load_bias, uint64_t start_offset,
            uint64_t total_entries) : memory_(memory), load_bias_(load_bias)
            , start_offset_(start_offset), total_entries_(total_entries) {}

    bool FindEntry(uint32_t pc, uint64_t* entry_offset);

    bool Step(uint64_t pc, uintptr_t* regs, unwindstack::Memory* process_memory, bool* finished);

    bool GenerateQuickenTable(unwindstack::Memory* process_memory);

    uint64_t GetLoadBias();

    unwindstack::Memory* Memory() {
        return memory_;
    };

    QutErrorCode last_error_code_ = QUT_ERROR_NONE;
    size_t bad_entries_ = 0;

protected:

    std::mutex lock_;

    unwindstack::Memory* memory_;
    uint64_t load_bias_ = 0;
    uint64_t start_offset_;
    uint64_t total_entries_;
    std::unique_ptr<QutSections> qut_sections_;
};

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_INTERFACE_H
