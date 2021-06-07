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

#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H

#include <jni.h>

#include <unwindstack/Elf.h>

#include "Errors.h"

typedef uintptr_t uptr;

namespace wechat_backtrace {

    QUT_EXTERN_C std::shared_ptr<unwindstack::Memory> &GetLocalProcessMemory();

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

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWINDER_H
