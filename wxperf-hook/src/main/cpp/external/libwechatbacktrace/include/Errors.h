#ifndef _LIBWECHATBACKTRACE_ERRORS_H
#define _LIBWECHATBACKTRACE_ERRORS_H

namespace wechat_backtrace {

enum QutErrorCode {

    // Unwind error.
    QUT_ERROR_NONE = 0,                 // No error.
    QUT_ERROR_UNWIND_INFO,              // Unable to use unwind information to unwind.
    QUT_ERROR_INVALID_MAP,              // Unwind in an invalid map.
    QUT_ERROR_MAX_FRAMES_EXCEEDED,      // The number of frames exceed the total allowed.
    QUT_ERROR_REPEATED_FRAME,           // The last frame has the same pc/sp as the next.
    QUT_ERROR_INVALID_ELF,              // Unwind in an invalid elf.
    QUT_ERROR_QUT_SECTION_INVALID,      // Quicken unwind table is invalid.
    QUT_ERROR_INVALID_QUT_INSTR,        // Met a invalid quicken instruction.

};

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_ERRORS_H
