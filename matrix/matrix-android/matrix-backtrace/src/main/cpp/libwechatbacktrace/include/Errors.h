/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBWECHATBACKTRACE_ERRORS_H
#define _LIBWECHATBACKTRACE_ERRORS_H

#include "BacktraceDefine.h"

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
    QUT_ERROR_MAPS_IS_NULL,             // Could not get maps

    QUT_ERROR_REQUEST_QUT_FILE_FAILED,  // Request QUT file failed.
    QUT_ERROR_REQUEST_QUT_INMEM_FAILED, // Request QUT in memory failed.
    QUT_ERROR_READ_STACK_FAILED,        // Read memory from stack failed.
    QUT_ERROR_TABLE_INDEX_OVERFLOW,     //
};

enum DwarfErrorCode : uint8_t {
    DWARF_ERROR_NONE,
    DWARF_ERROR_MEMORY_INVALID,
    DWARF_ERROR_ILLEGAL_VALUE,
    DWARF_ERROR_ILLEGAL_STATE,
    DWARF_ERROR_STACK_INDEX_NOT_VALID,
    DWARF_ERROR_NOT_IMPLEMENTED,
    DWARF_ERROR_TOO_MANY_ITERATIONS,
    DWARF_ERROR_CFA_NOT_DEFINED,
    DWARF_ERROR_UNSUPPORTED_VERSION,
    DWARF_ERROR_NO_FDES,

    DWARF_ERROR_EXPRESSION_NOT_SUPPORT,
    DWARF_ERROR_EXPRESSION_NOT_SUPPORT_BREG,
    DWARF_ERROR_EXPRESSION_NOT_SUPPORT_BREGX,
    DWARF_ERROR_EXPRESSION_REACH_BREG,
    DWARF_ERROR_EXPRESSION_REACH_BREGX,

    DWARF_ERROR_NOT_SUPPORT,

};

struct DwarfErrorData {
    DwarfErrorCode code;
    uint64_t address;
};

enum QutFileError : uint16_t {
    NoneError = 0,
    NotInitialized = 1,
    NotWarmedUp = 2,
    LoadRequesting = 3,
    OpenFileFailed = 4,
    FileStateError = 5,
    FileTooShort = 6,
    MmapFailed = 7,
    QutVersionNotMatch = 8,
    ArchNotMatch = 9,
    BuildIdNotMatch = 10,
    FileLengthNotMatch = 11,
    InsertNewQutFailed = 12,
    TryInvokeJavaRequestQutGenerate = 13,
    LoadFailed = 14,
};

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_ERRORS_H
