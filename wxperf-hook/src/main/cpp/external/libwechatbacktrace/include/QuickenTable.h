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
#include <include/unwindstack/Elf.h>
#include "unwindstack/Memory.h"
#include "Predefined.h"

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

    enum QutFileError : uint16_t {
        None,
        LoadRequesting,
        OpenFileFailed,
        FileStateError,
        FileTooShort,
        MmapFailed,
        QutVersionNotMatch,
        ArchNotMatch,
        FileLengthNotMatch,
    };

    class QuickenTableManager {

    private:
        QuickenTableManager() = default;

        ~QuickenTableManager() {};

        QuickenTableManager(const QuickenTableManager &);

        QuickenTableManager &operator=(const QuickenTableManager &);

        std::unordered_map<std::string, QutSections *> qut_sections_map_;          // TODO destruction
        std::unordered_map<std::string, bool> qut_sections_requesting_;   // TODO destruction

        std::mutex lock_;

        static std::string sSavingPath;     // TODO default value

    public:
        static QuickenTableManager &getInstance() {
            static QuickenTableManager instance;
            return instance;
        }

        static bool MakeDir(const char *path) {
            struct stat st = {0};
            if (stat(path, &st) == -1) {
                mkdir(path, 0700);
            }
        }

        static bool Init(std::string saving_path, unwindstack::ArchEnum arch) {
            // TODO Check security here
            std::string arch_folder = CURRENT_ARCH_ENUM == QUT_ARCH_ARM ? "arm32" : "arm64";
            sSavingPath = saving_path + FILE_SEPERATOR + arch_folder;
            MakeDir(sSavingPath.c_str());
        }

        QutFileError
        RequestQutSections(std::string soname, std::string build_id, QutSectionsPtr &qut_sections);

        QutFileError
        SaveQutSections(std::string soname, std::string build_id, QutSectionsPtr qut_sections);

        void InsertQutSections(std::string soname, std::string build_id, QutSectionsPtr qut_sections);
    };

    class QuickenTable {

    public:
        QuickenTable(QutSections *fut_sections, uintptr_t *regs, unwindstack::Memory *memory,
                     unwindstack::Memory *process_memory) : regs_(regs), memory_(memory),
                                                            process_memory_(process_memory),
                                                            fut_sections_(fut_sections) {}

        ~QuickenTable() {};

        bool Eval(size_t entry_offset);

        uptr cfa_ = 0;
        uptr dex_pc_ = 0;
        bool pc_set_ = false;

    protected:

        bool Decode32(const uint32_t *instructions, const size_t amount, const size_t start_pos);

        bool Decode64(const uint64_t *instructions, const size_t amount, const size_t start_pos);

        bool Decode(const uptr *instructions, const size_t amount, const size_t start_pos);

        uptr *regs_ = nullptr;

        unwindstack::Memory *memory_ = nullptr;
        unwindstack::Memory *process_memory_ = nullptr;
        QutSections *fut_sections_ = nullptr;
    };


}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H
