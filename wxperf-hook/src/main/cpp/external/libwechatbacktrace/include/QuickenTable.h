//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H

#include <cstdint>
#include <map>
#include "unwindstack/Memory.h"
#include "Types.h"

namespace wechat_backtrace {

#ifdef __arm__
#define QUT_TBL_ROW_SIZE 3  // 4 - 1
#else
#define QUT_TBL_ROW_SIZE 7  // 8 - 1
#endif

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

    uptr* quidx = nullptr;
    uptr* qutbl = nullptr;

    size_t idx_size = 0;
    size_t tbl_size = 0;

    size_t idx_capacity = 0;
    size_t tbl_capacity = 0;

    size_t total_entries = 0;
    size_t start_offset_ = 0;

    bool load_from_file = false;
};

class QuickenTableManager {

private:
    QuickenTableManager() = default;
    ~QuickenTableManager() {
        // TODO
    };
    QuickenTableManager(const QuickenTableManager&);
    QuickenTableManager& operator=(const QuickenTableManager&);

    // build_id, pair(soname, QutSections)
    std::map<std::string, std::pair<std::string, std::shared_ptr<QutSections>>> qut_sections_map_;

    std::mutex lock_;

public:
    static QuickenTableManager& getInstance() {
        static QuickenTableManager instance;
        return instance;
    }

    std::shared_ptr<QutSections> LoadQutSections(std::string sopath, std::string build_id);

    void SaveQutSections(std::string sopath, std::string build_id, std::shared_ptr<QutSections> qut_sections);

};

class QuickenTable {

public:
    QuickenTable(QutSections* fut_sections, uintptr_t* regs, unwindstack::Memory* memory,
            unwindstack::Memory* process_memory) : regs_(regs), memory_(memory),
            process_memory_(process_memory), fut_sections_(fut_sections) {}
    ~QuickenTable() {};

    bool Eval(size_t entry_offset);

    uptr cfa_ = 0;
    uptr dex_pc_ = 0;
    bool pc_set_ = false;

protected:

    bool Decode32(const uint32_t * instructions, const size_t amount, const size_t start_pos);
    bool Decode64(const uint64_t * instructions, const size_t amount, const size_t start_pos);
    bool Decode(const uptr * instructions, const size_t amount, const size_t start_pos);

    uptr * regs_ = nullptr;

    unwindstack::Memory* memory_ = nullptr;
    unwindstack::Memory* process_memory_ = nullptr;
    QutSections* fut_sections_ = nullptr;
};


}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H
