#ifndef _LIBWECHATBACKTRACE_QUICKEN_INTERFACE_H
#define _LIBWECHATBACKTRACE_QUICKEN_INTERFACE_H

#include <stdint.h>
#include <memory>
#include <mutex>

#include <unwindstack/Memory.h>
#include <unwindstack/Error.h>
#include "QuickenTableGenerator.h"
#include "Errors.h"
#include "QuickenMaps.h"
#include "QuickenUtility.h"

namespace wechat_backtrace {

    class QuickenMapInfo;

    class QuickenInterface {

    public:
        QuickenInterface(/* unwindstack::Memory *memory,*/ uint64_t load_bias,
                                                           uint64_t elf_offset,
                                                           unwindstack::ArchEnum arch)
                : /* memory_(memory), */ load_bias_(load_bias), elf_offset_(elf_offset),
                                         arch_(arch) {}

        bool FindEntry(uptr pc, size_t *entry_offset);

        bool Step(uptr pc, uptr *regs, unwindstack::Memory *process_memory, uptr stack_top,
                  uptr stack_bottom, uptr frame_size, uint64_t *dex_pc, bool *finished);

        bool StepBack(uptr pc, uptr sp, uptr fp, uint8_t &regs_bits, uptr *regs,
                      unwindstack::Memory *process_memory, bool *finish);

        template<typename AddressType>
        bool GenerateQuickenTable(unwindstack::Memory *memory, unwindstack::Memory *process_memory,
                                  QutSectionsPtr qut_sections);

        bool TryInitQuickenTable();

        uint64_t GetLoadBias();

        uint64_t GetElfOffset();

        void SetArmExidxInfo(uint64_t start_offset, uint64_t total_entries) {
            arm_exidx_info_ = {start_offset, 0, total_entries};
        }

        void SetEhFrameHdrInfo(uint64_t offset, int64_t section_bias, uint64_t size) {
            eh_frame_hdr_info_ = {offset, section_bias, size};
        }

        void SetEhFrameInfo(uint64_t offset, int64_t section_bias, uint64_t size) {
            eh_frame_info_ = {offset, section_bias, size};
        }

        void SetDebugFrameInfo(uint64_t offset, int64_t section_bias, uint64_t size) {
            debug_frame_info_ = {offset, section_bias, size};
        }

        void SetGnuEhFrameHdrInfo(uint64_t offset, int64_t section_bias, uint64_t size) {
            gnu_eh_frame_hdr_info_ = {offset, section_bias, size};
        }

        void SetGnuEhFrameInfo(uint64_t offset, int64_t section_bias, uint64_t size) {
            gnu_eh_frame_info_ = {offset, section_bias, size};
        }

        void SetGnuDebugFrameInfo(uint64_t offset, int64_t section_bias, uint64_t size) {
            gnu_debug_frame_info_ = {offset, section_bias, size};
        }

        void
        SetSoInfo(const std::string &sopath, const std::string &soname,
                  const std::string &build_id_hex) {
            soname_ = soname;
            sopath_ = sopath;
            build_id_hex_ = build_id_hex;
            build_id_ = ToBuildId(build_id_hex);
            hash_ = ToHash(sopath_);
        }

        QutSections *GetQutSections() {
            return qut_sections_;
        }

        QutErrorCode last_error_code_ = QUT_ERROR_NONE;
        size_t bad_entries_ = 0;    // TODO

        bool log = false;
        uptr log_pc = 0;


    protected:

        std::mutex lock_;

        std::string soname_;
        std::string sopath_;
        std::string build_id_;
        std::string build_id_hex_;
        std::string hash_;

//        unwindstack::Memory *memory_;
        uint64_t load_bias_ = 0;
        uint64_t elf_offset_ = 0;

        unwindstack::ArchEnum arch_;

        FrameInfo arm_exidx_info_ = {0};

        FrameInfo eh_frame_hdr_info_ = {0};
        FrameInfo eh_frame_info_ = {0};
        FrameInfo debug_frame_info_ = {0};

        FrameInfo gnu_eh_frame_hdr_info_ = {0};
        FrameInfo gnu_eh_frame_info_ = {0};
        FrameInfo gnu_debug_frame_info_ = {0};

        QutSections *qut_sections_ = nullptr;
    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_INTERFACE_H
