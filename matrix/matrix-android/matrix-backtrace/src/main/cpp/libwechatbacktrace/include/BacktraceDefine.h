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

#ifndef _LIBWECHATBACKTRACE_DEFINE_H
#define _LIBWECHATBACKTRACE_DEFINE_H

#include <memory>
#include <unwindstack/Elf.h>

#include <Predefined.h>

// *** Version ***
#define QUT_VERSION 0x1
#define QUT_NATIVE_ONLY_MASK 0x1000000
#define QUT_NATIVE_ONLY_VERSION (QUT_NATIVE_ONLY_MASK | QUT_VERSION)

#define MAX_FRAME_SHORT 16
#define MAX_FRAME_NORMAL 32
#define MAX_FRAME_LONG 64
#define MAX_FRAME_LONG_LONG 80

#define QUT_ARCH_ARM 0x1
#define QUT_ARCH_ARM64 0x2

#ifdef __cplusplus__
#define QUT_EXTERN_C extern "C"
#define QUT_EXTERN_C_BLOCK extern "C" {
#define QUT_EXTERN_C_BLOCK_END }
};
#else
#define QUT_EXTERN_C
#define QUT_EXTERN_C_BLOCK
#define QUT_EXTERN_C_BLOCK_END
#endif

#ifdef __arm__
#define CURRENT_ARCH unwindstack::ArchEnum::ARCH_ARM
#define CURRENT_ARCH_ENUM QUT_ARCH_ARM

#define QUT_TBL_ROW_SIZE 3  // 4 - 1
#else
#define CURRENT_ARCH unwindstack::ArchEnum::ARCH_ARM64
#define CURRENT_ARCH_ENUM QUT_ARCH_ARM64

#define QUT_TBL_ROW_SIZE 7  // 8 - 1
#endif

#define BACKTRACE_INITIALIZER(MAX_FRAMES) \
    {MAX_FRAMES, 0, std::shared_ptr<wechat_backtrace::Frame>( \
    new wechat_backtrace::Frame[MAX_FRAMES], std::default_delete<wechat_backtrace::Frame[]>())}

#define MEMORY_USAGE_UPPER_BOUND 9 * 1000 * 1000 // Amount of instructions, 900w
#define CHECK_MEMORY_OVERWHELMED(USAGE) (USAGE > MEMORY_USAGE_UPPER_BOUND)

namespace wechat_backtrace {

    typedef uintptr_t uptr;

    constexpr auto FILE_SEPARATOR = "/";

#ifdef __arm__
    typedef uint32_t addr_t;
#else
    typedef uint64_t addr_t;
#endif

    typedef unsigned long long int ullint_t;
    typedef long long int llint_t;

    typedef unsigned long int ulint_t;
    typedef long int lint_t;

#define frame_attr_is_dex_pc    ((unsigned) 0x1)
#define frame_attr_maybe_java   ((unsigned) 0x2)

#define set_frame_attr_is_dex_pc(frame) (frame.attr |= frame_attr_is_dex_pc)
#define set_frame_attr_maybe_java(frame) (frame.attr |= frame_attr_maybe_java)
#define set_frame_attr_all(frame) (frame.attr |= (frame_attr_is_dex_pc | frame_attr_maybe_java))
#define is_frame_attr_is_dex_pc(frame) (frame.attr & frame_attr_is_dex_pc)
#define is_frame_attr_maybe_java(frame) (frame.attr & frame_attr_maybe_java)

#if defined(__aarch64__)
    struct __attribute__((__packed__)) Frame {
        unsigned char attr : 8;
        uptr pc : 56;
    };
#else
    struct __attribute__((__packed__)) Frame {
        unsigned char attr;
        uptr pc;
    };
#endif

    struct FrameDetail {
        const uptr rel_pc;
        const char *map_name;
        const char *function_name;
    };

    struct Backtrace {
        size_t max_frames = 0;
        size_t frame_size = 0;
        std::shared_ptr<Frame> frames;
    };

    template <size_t _Max>
    struct BacktraceFixed {
        Frame frames[_Max];
        uint8_t frame_size = 0;
    };

    enum BacktraceMode {
        FramePointer = 0,
        Quicken = 1,
        DwarfBased = 2
    };

    struct FrameElement {
        uint64_t rel_pc = 0;
        uint64_t map_offset = 0;
        uint64_t function_offset = 0;

        std::string map_name;
        std::string function_name;
        std::string build_id;

        bool maybe_java = false;
    };

    struct QuickenContext {
        uptr stack_bottom;
        uptr stack_top;
        uptr *regs;
        uptr frame_max_size;
        wechat_backtrace::Frame *backtrace;
        uptr frame_size;
        bool update_maps_as_need;
    };

    struct StepContext {
        const uptr stack_bottom;
        const uptr stack_top;
        uptr *regs;
        uptr pc;
        uptr dex_pc;
        uptr frame_index;
        bool finished;
    };

    struct QuickenGenerationContext {

        uint16_t regs_total;
        bool native_only;

        uint64_t estimate_memory_usage;
        bool memory_overwhelmed;
    };

    typedef bool(*quicken_generate_delegate_func)(const std::string &, const uint64_t, const bool);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_DEFINE_H
