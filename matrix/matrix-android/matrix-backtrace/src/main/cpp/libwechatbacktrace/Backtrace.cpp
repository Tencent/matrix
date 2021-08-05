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

#include <dlfcn.h>
#include "Backtrace.h"
#include <FpUnwinder.h>
#include <MinimalRegs.h>
#include <QuickenUnwinder.h>
#include <QuickenMaps.h>
#include <LocalMaps.h>
#include <QuickenTableManager.h>
#include <android-base/macros.h>
#include <android-base/stringprintf.h>
#include <cxxabi.h>
#include <DebugDexFiles.h>
#include <PthreadExt.h>

#define WECHAT_BACKTRACE_TAG "Wechat.Backtrace"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    static BacktraceMode backtrace_mode = FramePointer;
    static bool quicken_unwind_always_enabled = false;

#ifdef __aarch64__
    static const bool m_is_arm32 = false;
#else
    static const bool m_is_arm32 = true;
#endif

    BACKTRACE_EXPORT
    BacktraceMode get_backtrace_mode() {
        return backtrace_mode;
    }

    BACKTRACE_EXPORT
    void set_backtrace_mode(BacktraceMode mode) {
        backtrace_mode = mode;
    }

    BACKTRACE_EXPORT
    void set_quicken_always_enable(bool enable) {
        quicken_unwind_always_enabled = enable;
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(restore_frame_detail)(const Frame *frames, const size_t frame_size,
                                                 const std::function<void(
                                                         FrameDetail)> &frame_callback) {
        LOGD(WECHAT_BACKTRACE_TAG, "Restore frame data size: %zu.", frame_size);
        if (frames == nullptr || frame_callback == nullptr) {
            return;
        }

        for (size_t i = 0; i < frame_size; i++) {
            auto &frame_data = frames[i];
            Dl_info stack_info{};
            int success = dladdr((void *) frame_data.pc,
                                 &stack_info); // 用修正后的 pc dladdr 会偶现 npe crash, 因此还是用 lr

#ifdef __aarch64__  // TODO Fix hardcode
            // fp_unwind 得到的 pc 除了第 0 帧实际都是 LR, arm64 指令长度都是定长 32bit, 所以 -4 以恢复 pc
            uptr real_pc = frame_data.pc - (i > 0 ? 4 : 0);
#else
            uptr real_pc = frame_data.pc;
#endif
            FrameDetail detail = {
                    .rel_pc = real_pc - (uptr) stack_info.dli_fbase,
                    .map_name = success == 0 || stack_info.dli_fname == nullptr ? "null"
                                                                                : stack_info.dli_fname,
                    .function_name = success == 0 || stack_info.dli_sname == nullptr ? "null"
                                                                                     : stack_info.dli_sname
            };
            frame_callback(detail);
        }

    }

    BACKTRACE_EXPORT void
    quicken_frame_format(wechat_backtrace::FrameElement &frame_element, size_t num,
                         std::string &data) {

        if (m_is_arm32) {
            data += android::base::StringPrintf("  #%02zu pc %08" PRIx64, num,
                                                (uint64_t) frame_element.rel_pc);
        } else {
            data += android::base::StringPrintf("  #%02zu pc %016" PRIx64, num,
                                                (uint64_t) frame_element.rel_pc);
        }

        if (!frame_element.map_name.empty()) {
            // No valid map associated with this frame.
            data += "  " + frame_element.map_name;
            if (frame_element.map_offset != 0) {
                data += android::base::StringPrintf(" (offset 0x%" PRIx64 ")",
                                                    frame_element.map_offset);
            }
        }

        if (!frame_element.function_name.empty()) {
            data += " (" + frame_element.function_name;
            if (frame_element.function_offset != 0) {
                data += android::base::StringPrintf("+%" PRId64, frame_element.function_offset);
            }
            data += ')';
        }

        if (!frame_element.build_id.empty()) {
            data += " (BuildId: " + frame_element.build_id + ')';
        }
    }

    BACKTRACE_EXPORT inline void
    to_quicken_frame_element(Frame &frame, unwindstack::MapInfo *map_info,
                             wechat_backtrace::ElfWrapper *elf_wrapper,
                             bool fill_map_info, bool fill_build_id,
                             FrameElement &frame_element) {

        frame_element.rel_pc = frame.rel_pc;
        frame_element.maybe_java = frame.maybe_java;

        if (fill_map_info) {
            if (map_info == nullptr) {
                // No valid map associated with this frame.
                frame_element.map_name = "<unknown>";
            } else if (elf_wrapper != nullptr && !elf_wrapper->GetSoname().empty()) {
                frame_element.map_name = elf_wrapper->GetSoname();
            } else if (!map_info->name.empty()) {
                frame_element.map_name = map_info->name;
            } else {
                frame_element.map_name = android::base::StringPrintf(
                        "  <anonymous:%" PRIx64 ">", map_info->start);
            }
        }

        if (!frame_element.function_name.empty()) {
            if (!frame.maybe_java) {
                char *demangled_name = abi::__cxa_demangle(frame_element.function_name.c_str(),
                                                           nullptr, nullptr,
                                                           nullptr);
                if (demangled_name != nullptr) {
                    frame_element.function_name = demangled_name;
                    free(demangled_name);
                }
            }
        }

        if (fill_build_id && map_info != nullptr) {
            std::string build_id = elf_wrapper->GetBuildId();
            if (!build_id.empty()) {
                frame_element.build_id = build_id;
            }
        }
    }

    BACKTRACE_EXPORT void
    get_stacktrace_elements(wechat_backtrace::Frame *frames, const size_t frame_size,
                            bool shrunk_java_stacktrace,
                            FrameElement *stacktrace_elements, const size_t max_elements,
                            size_t &elements_size) {
        std::shared_ptr<wechat_backtrace::Maps> quicken_maps = wechat_backtrace::Maps::current();
        auto process_memory = wechat_backtrace::GetLocalProcessMemory();
        auto dex_debug = wechat_backtrace::DebugDexFiles::Instance();
        auto jit_debug = wechat_backtrace::DebugJit::Instance();
        bool found_java_frame = false;
        wechat_backtrace::QuickenMapInfo *last_map_info = nullptr;
        elements_size = 0;
        for (size_t num = 0;
             num < frame_size && elements_size < max_elements; num++) {

            if (shrunk_java_stacktrace) {
                if (found_java_frame && !frames[num].maybe_java) {
                    continue;
                }
                if (found_java_frame != frames[num].maybe_java) {
                    found_java_frame = frames[num].maybe_java;
                }
            }
            wechat_backtrace::QuickenMapInfo *map_info;
            if (last_map_info != nullptr && frames[num].pc >= last_map_info->start &&
                frames[num].pc < last_map_info->end) {
                map_info = last_map_info;
            } else {
                map_info = quicken_maps->Find(frames[num].pc);
                last_map_info = map_info;
            }

            auto frame_element = &stacktrace_elements[elements_size++];
            std::string *function_name = &frame_element->function_name;
            uint64_t *function_offset = &frame_element->function_offset;
            wechat_backtrace::ElfWrapper *elf_wrapper = nullptr;
            if (map_info != nullptr) {

                if (frames[num].is_dex_pc) {
                    frames[num].rel_pc = frames[num].pc - map_info->start;

                    dex_debug->GetMethodInformation(quicken_maps.get(), map_info, frames[num].pc,
                                                    function_name,
                                                    function_offset);
                } else {
                    auto interface = map_info->GetQuickenInterface(process_memory);
                    if (interface && interface->elf_wrapper_) {
                        elf_wrapper = interface->elf_wrapper_.get();
                        if (!elf_wrapper->IsJitCache()) {
                            interface->elf_wrapper_->GetFunctionName(frames[num].rel_pc,
                                                                     function_name,
                                                                     function_offset);
                        } else {
                            unwindstack::Elf *jit_elf = jit_debug->GetElf(quicken_maps.get(),
                                                                          frames[num].pc);
                            if (jit_elf) {
                                jit_elf->GetFunctionName(frames[num].pc, function_name,
                                                         function_offset);
                            }
                        }
                    }
                }
                frame_element->map_offset = map_info->elf_start_offset;
            }

            to_quicken_frame_element(
                    frames[num], map_info, elf_wrapper,
                    /* fill_map_info */ !shrunk_java_stacktrace || !frames[num].maybe_java,
                    /* fill_build_id */ false,
                    *frame_element);
        }
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(quicken_unwind)(QuickenContext *context) {

        if (context->stack_top == 0 && context->stack_bottom == 0) {
            pthread_attr_t attr;
            pthread_getattr_ext(pthread_self(), &attr);
            context->stack_bottom = reinterpret_cast<uptr>(attr.stack_base);
            context->stack_top = reinterpret_cast<uptr>(attr.stack_base) + attr.stack_size;
        }

        WeChatQuickenUnwind(context);
    }

    inline void
    quicken_based_unwind_inlined(Frame *frames, const size_t max_frames,
                                 size_t &frame_size) {

        uptr regs[QUT_MINIMAL_REG_SIZE];
        GetQuickenMinimalRegs(regs);
        pthread_attr_t attr;
        pthread_getattr_ext(pthread_self(), &attr);
        QuickenContext context = {
                .stack_bottom = reinterpret_cast<uptr>(attr.stack_base),
                .stack_top = reinterpret_cast<uptr>(attr.stack_base) + attr.stack_size,
                .regs = regs,
                .frame_max_size = max_frames,
                .backtrace = frames,
                .frame_size = 0,
                .update_maps_as_need = false,
                .native_only = false
        };
        WeChatQuickenUnwind(&context);

        frame_size = context.frame_size;
    }

    inline void
    fp_based_unwind_inlined(Frame *frames, const size_t max_frames,
                            size_t &frame_size) {
        uptr regs[FP_MINIMAL_REG_SIZE];
        GetFramePointerMinimalRegs(regs);
        FpUnwind(regs, frames, max_frames, frame_size);
    }

    inline void
    dwarf_based_unwind_inlined(Frame *frames, const size_t max_frames,
                               size_t &frame_size) {
        std::vector<unwindstack::FrameData> dst;
        unwindstack::RegsArm regs;
        RegsGetLocal(&regs);
        BACKTRACE_FUNC_WRAPPER(dwarf_unwind)(&regs, dst, max_frames);
        size_t i = 0;
        auto it = dst.begin();
        while (it != dst.end()) {
            frames[i].pc = it->pc;
            frames[i].is_dex_pc = it->is_dex_pc;
            i++;
            it++;
        }
        frame_size = dst.size();
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(quicken_based_unwind)(Frame *frames, const size_t max_frames,
                                                 size_t &frame_size) {
        quicken_based_unwind_inlined(frames, max_frames, frame_size);
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(fp_based_unwind)(Frame *frames, const size_t max_frames,
                                            size_t &frame_size) {
        fp_based_unwind_inlined(frames, max_frames, frame_size);
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(dwarf_based_unwind)(Frame *frames, const size_t max_frames,
                                               size_t &frame_size) {
        dwarf_based_unwind_inlined(frames, max_frames, frame_size);
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(unwind_adapter)(Frame *frames, const size_t max_frames,
                                           size_t &frame_size) {

        switch (get_backtrace_mode()) {
            case FramePointer:
                fp_based_unwind_inlined(frames, max_frames, frame_size);
                break;
            case Quicken:
                quicken_based_unwind_inlined(frames, max_frames, frame_size);
                break;
            case DwarfBased:
                dwarf_based_unwind_inlined(frames, max_frames, frame_size);
                break;
        }
    }

    BACKTRACE_EXPORT void BACKTRACE_FUNC_WRAPPER(notify_maps_changed)() {
        if (backtrace_mode == DwarfBased) {
            wechat_backtrace::UpdateLocalMaps();
        }
        if (quicken_unwind_always_enabled || backtrace_mode == Quicken) {
            // Parse quicken maps
            wechat_backtrace::Maps::Parse();
        }
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(dwarf_unwind)(unwindstack::Regs *regs,
                                         std::vector<unwindstack::FrameData> &dst,
                                         size_t frameSize) {

        std::shared_ptr<unwindstack::LocalMaps> local_maps = GetMapsCache();
        if (!local_maps) {
            LOGE(WECHAT_BACKTRACE_TAG, "Err: unable to get maps.");
            return;
        }

        auto process_memory = GetLocalProcessMemory();
        unwindstack::Unwinder unwinder(frameSize, local_maps.get(), regs, process_memory);
        auto jit_debug = wechat_backtrace::GetJitDebug(process_memory);
        unwinder.SetJitDebug(jit_debug.get(), regs->Arch());
        unwinder.SetResolveNames(false);
        unwinder.Unwind();

        dst = unwinder.frames();
    }

    BACKTRACE_EXPORT void
    BACKTRACE_FUNC_WRAPPER(fp_unwind)(uptr *regs, Frame *frames, const size_t frameMaxSize,
                                      size_t &frameSize) {
        FpUnwind(regs, frames, frameMaxSize, frameSize);
    }

    QUT_EXTERN_C_BLOCK_END
}
