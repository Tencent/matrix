/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H
#define _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H

#include <cstdint>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "unwindstack/Elf.h"
#include "unwindstack/Memory.h"
#include "BacktraceDefine.h"
#include "Errors.h"
#include "Log.h"

namespace wechat_backtrace {

    struct QutSections {

        QutSections() = default;

        ~QutSections() {
            if (!load_from_file) {
                if (quidx) {
                    delete quidx;
                }
                if (qutbl) {
                    delete qutbl;
                }
            } else {
                if (mmap_ptr) {
                    munmap(mmap_ptr, map_size);
                }
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

        void *mmap_ptr = nullptr;
        size_t map_size = 0;

        bool load_from_file = false;
    };

    struct QutSectionsInMemory : QutSections {
        uint64_t pc_start;
        uint64_t pc_end;
    };

    typedef QutSections *QutSectionsPtr;
    typedef QutSectionsInMemory *QutSectionsInMemoryPtr;

    class QuickenTable {

    public:
        QuickenTable(QutSections *qut_sections, uintptr_t *regs,
                     unwindstack::Memory *process_memory, uptr stack_top, uptr stack_bottom,
                     uptr frame_size)
                : regs_(regs), process_memory_(process_memory),
                  qut_sections_(qut_sections), stack_top_(stack_top),
                  stack_bottom_(stack_bottom), frame_size(frame_size) {};

        ~QuickenTable() {};

        QutErrorCode Eval(size_t entry_offset);

        uptr cfa_ = 0;
        uptr dex_pc_ = 0;
        bool pc_set_ = false;

        const bool log = false;
        const size_t log_entry = 0;
//        bool log = false;
//        size_t log_entry = 646;

    protected:

        QutErrorCode
        Decode32(const uint32_t *instructions, const size_t amount, const size_t start_pos);

        QutErrorCode
        Decode64(const uint64_t *instructions, const size_t amount, const size_t start_pos);

        QutErrorCode Decode(const uptr *instructions, const size_t amount, const size_t start_pos);

        bool ReadStack(const uptr addr, uptr *value);

        uptr *regs_ = nullptr;

        unwindstack::Memory *process_memory_ = nullptr;
        QutSections *qut_sections_ = nullptr;

        const uptr stack_top_;
        const uptr stack_bottom_;
        uptr frame_size;
    };


}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UNWIND_TABLE_H
