#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H

#include <jni.h>

#include <unwindstack/Elf.h>

#include "Errors.h"

typedef uintptr_t uptr;

namespace wechat_backtrace {

    QUT_EXTERN_C void
    StatisticWeChatQuickenUnwindTable(
            const std::string &sopath, std::vector<uint32_t> &processed_result);

    QUT_EXTERN_C bool
    GenerateQutForLibrary(
            const std::string &sopath,
            const uint64_t elf_start_offset,
            const bool only_save_file);

    QUT_EXTERN_C void
    NotifyWarmedUpQut(
            const std::string &sopath,
            const uint64_t elf_start_offset);

    QUT_EXTERN_C bool
    TestLoadQut(
            const std::string &so_path,
            const uint64_t elf_start_offset);

    QUT_EXTERN_C std::vector<std::string> ConsumeRequestingQut();

    QUT_EXTERN_C QutErrorCode
    WeChatQuickenUnwind(
            const unwindstack::ArchEnum arch, uptr *regs,
            const uptr frame_max_size,
            Frame *backtrace, uptr &frame_size);

//    QUT_EXTERN_C QutErrorCode
//    WeChatQuickenUnwindV2_WIP(unwindstack::ArchEnum arch, uptr *regs, uptr *backtrace,
//                              uptr frame_max_size, uptr &frame_size);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
