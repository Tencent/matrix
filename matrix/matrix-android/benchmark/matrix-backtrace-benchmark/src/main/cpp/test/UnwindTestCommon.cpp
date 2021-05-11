#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include <inttypes.h>
#include <MinimalRegs.h>
#include <QuickenUnwinder.h>
#include <QuickenMaps.h>
#include <LocalMaps.h>
#include "Backtrace.h"
#include "UnwindTestCommon.h"
#include "BenchmarkLog.h"
#include "BacktraceDefine.h"
#include "../../../../../../matrix-backtrace/src/main/cpp/external/libunwindstack/deps/android-base/include/android-base/stringprintf.h"

#define DWARF_UNWIND_TAG "Dwarf-Unwind"
#define FP_UNWIND_TAG "Fp-Unwind"
#define WECHAT_BACKTRACE_TAG "WeChat-Quicken-Unwind"

//#define FRAME_MAX_SIZE 18 // Native only
#define FRAME_MAX_SIZE 60 // Through native/JNI/AOT

#define TEST_NanoSeconds_Start(timestamp) \
        long timestamp = 0; \
        { \
            struct timespec tms; \
            if (clock_gettime(CLOCK_REALTIME, &tms)) { \
                BENCHMARK_LOGE(UNWIND_TEST_TAG, "Err: Get time failed."); \
            } else { \
                timestamp = tms.tv_nsec; \
            } \
        }

#define TEST_NanoSeconds_End(tag, timestamp, frames) \
        { \
            struct timespec tms; \
            if (clock_gettime(CLOCK_REALTIME, &tms)) { \
                BENCHMARK_LOGE(UNWIND_TEST_TAG, "Err: Get time failed."); \
            } \
            long duration = (tms.tv_nsec - timestamp); \
            duration = duration == 0 ? 100 : duration; /* At least 100ns */\
            BENCHMARK_LOGE(UNWIND_TEST_TAG, #tag" %ld(ns) - %ld(ns) = costs: %ld(ns), frames = %zu" \
                , tms.tv_nsec, timestamp, duration, (size_t) frames); \
            benchmark_counting(duration); \
        }

#ifdef __cplusplus
extern "C" {
#endif

static UnwindTestMode gMode = DWARF_UNWIND;

void set_unwind_mode(UnwindTestMode mode) {
    gMode = mode;
}

uint64_t sTotalDuration = 0;
size_t sBenchmarkTimes = 0;

void reset_benchmark_counting() {
    sTotalDuration = 0;
    sBenchmarkTimes = 0;
}

void benchmark_counting(uint64_t duration) {
    sTotalDuration += duration;
    sBenchmarkTimes++;
}

void dump_benchmark_calculation(const char *tag) {
    BENCHMARK_LOGE(UNWIND_TEST_TAG,
                   "%s Accumulated duration = %llu, times = %zu, avg = %llu, per-frame = %llu.",
                   tag, (unsigned long long) sTotalDuration, sBenchmarkTimes,
                   (unsigned long long) (sTotalDuration / sBenchmarkTimes),
                   (unsigned long long) (((unsigned long long) (sTotalDuration / sBenchmarkTimes)) /
                                         FRAME_MAX_SIZE));
}

void dwarf_format_frame(const unwindstack::FrameData &frame, unwindstack::MapInfo *map_info,
                        unwindstack::Elf *elf, bool is32Bit, std::string &data) {
    if (is32Bit) {
        data += android::base::StringPrintf("  #%02zu pc %08" PRIx64, frame.num,
                                            (uint64_t) frame.rel_pc);
    } else {
        data += android::base::StringPrintf("  #%02zu pc %016" PRIx64, frame.num,
                                            (uint64_t) frame.rel_pc);
    }

    if (frame.map_start == frame.map_end) {
        // No valid map associated with this frame.
        data += "  <unknown>";
    } else if (elf != nullptr && !elf->GetSoname().empty()) {
        data += "  " + elf->GetSoname();
    } else if (!map_info->name.empty()) {
        data += "  " + map_info->name;
    } else {
        data += android::base::StringPrintf("  <anonymous:%" PRIx64 ">", frame.map_start);
    }

    if (frame.map_elf_start_offset != 0) {
        data += android::base::StringPrintf(" (offset 0x%" PRIx64 ")", frame.map_elf_start_offset);
    }

    if (!frame.function_name.empty()) {
        char *demangled_name = abi::__cxa_demangle(frame.function_name.c_str(), nullptr, nullptr,
                                                   nullptr);
        if (demangled_name == nullptr) {
            data += " (" + frame.function_name;
        } else {
            data += " (";
            data += demangled_name;
            free(demangled_name);
        }
        if (frame.function_offset != 0) {
            data += android::base::StringPrintf("+%" PRId64, frame.function_offset);
        }
        data += ')';
    }

    if (map_info != nullptr) {
        std::string build_id = map_info->GetPrintableBuildID();
        if (!build_id.empty()) {
            data += " (BuildId: " + build_id + ')';
        }
    }
}

void fp_format_frame(wechat_backtrace::Frame &frame, size_t num, bool is32Bit, std::string &data) {

    Dl_info stack_info;
    dladdr((void *) frame.pc, &stack_info);

    uptr rel_pc = frame.pc - (uptr) stack_info.dli_fbase;
    if (is32Bit) {
        data += android::base::StringPrintf("  #%02zu pc %08" PRIx64, num, (uint64_t) rel_pc);
    } else {
        data += android::base::StringPrintf("  #%02zu pc %016" PRIx64, num, (uint64_t) rel_pc);
    }

    if (stack_info.dli_fbase == 0) {
        // No valid map associated with this frame.
        data += "  <unknown>";
    } else if (stack_info.dli_fname) {
        std::string so_name = std::string(stack_info.dli_fname);
        data += "  " + so_name;
    } else {
        data += android::base::StringPrintf("  <anonymous:%" PRIx64 ">",
                                            (uint64_t) stack_info.dli_fbase);
    }

    if (stack_info.dli_sname) {
        char *demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, nullptr,
                                                   nullptr);
        if (demangled_name == nullptr) {
            data += " (";
            data += stack_info.dli_sname;
        } else {
            data += " (";
            data += demangled_name;
            free(demangled_name);
        }
        if (stack_info.dli_saddr != 0) {
            uptr offset = (uptr) stack_info.dli_saddr - (uptr) stack_info.dli_fbase;
            data += android::base::StringPrintf("+%" PRId64, (uint64_t) offset);
        }
        data += ')';
    }

}

void
quicken_format_frame(wechat_backtrace::Frame &frame, unwindstack::MapInfo *map_info,
                     unwindstack::Elf *elf, size_t num, bool is32Bit, std::string &function_name,
                     uint64_t function_offset, std::string &data) {

    if (is32Bit) {
        data += android::base::StringPrintf("  #%02zu pc %08" PRIx64, num, (uint64_t) frame.rel_pc);
    } else {
        data += android::base::StringPrintf("  #%02zu pc %016" PRIx64, num,
                                            (uint64_t) frame.rel_pc);
    }

    if (map_info == nullptr) {
        // No valid map associated with this frame.
        data += "  <unknown>";
    } else if (elf != nullptr && !elf->GetSoname().empty()) {
        data += "  " + elf->GetSoname();
    } else if (!map_info->name.empty()) {
        data += "  " + map_info->name;
    } else {
        data += android::base::StringPrintf("  <anonymous:%" PRIx64 ">", map_info->start);
    }

    if (map_info) {
        if (map_info->elf_start_offset != 0) {
            data += android::base::StringPrintf(" (offset 0x%" PRIx64 ")",
                                                map_info->elf_start_offset);
        }
    }

    if (!function_name.empty()) {
        char *demangled_name = abi::__cxa_demangle(function_name.c_str(), nullptr, nullptr,
                                                   nullptr);
        if (demangled_name == nullptr) {
            data += " (" + function_name;
        } else {
            data += " (";
            data += demangled_name;
            free(demangled_name);
        }
        if (function_offset != 0) {
            data += android::base::StringPrintf("+%" PRId64, function_offset);
        }
        data += ')';
    }

    if (map_info != nullptr) {
        std::string build_id = map_info->GetPrintableBuildID();
        if (!build_id.empty()) {
            data += " (BuildId: " + build_id + ')';
        }
    }
}

inline void print_dwarf_unwind() {

    auto *frames = new std::vector<unwindstack::FrameData>;

    TEST_NanoSeconds_Start(nano);

    std::shared_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
    unwindstack::RegsGetLocal(regs.get());

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(dwarf_unwind)(regs.get(), *frames, FRAME_MAX_SIZE);

    TEST_NanoSeconds_End(unwindstack::dwarf_unwind, nano, frames->size());

    BENCHMARK_LOGE(DWARF_UNWIND_TAG, "frames = %"
            PRIuPTR, frames->size());

    wechat_backtrace::UpdateLocalMaps();
    std::shared_ptr<unwindstack::LocalMaps> local_maps = wechat_backtrace::GetMapsCache();
    if (!local_maps) {
        BENCHMARK_LOGE(DWARF_UNWIND_TAG, "Err: unable to get maps.");
        return;
    }
    auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
    unwindstack::JitDebug jit_debug(process_memory);
    jit_debug.SetArch(regs->Arch());

    size_t num = 0;
    for (auto p_frame = frames->begin(); p_frame != frames->end(); ++p_frame, num++) {

        unwindstack::MapInfo *map_info = local_maps->Find(p_frame->pc);
        unwindstack::Elf *elf = nullptr;
        if (map_info) {
            elf = map_info->GetElf(process_memory, regs->Arch());
            elf->GetFunctionName(p_frame->rel_pc, &p_frame->function_name,
                                 &p_frame->function_offset);
            if (!elf->valid()) {
                unwindstack::Elf *jit_elf = jit_debug.GetElf(local_maps.get(), p_frame->pc);
                if (jit_elf) {
                    jit_elf->GetFunctionName(p_frame->pc, &p_frame->function_name,
                                         &p_frame->function_offset);
                }
            }
        }
        std::string formatted;
        dwarf_format_frame(*p_frame, map_info, elf, regs->Arch() == unwindstack::ARCH_ARM,
                           formatted);

        BENCHMARK_LOGE(DWARF_UNWIND_TAG, formatted.c_str(), "");
    }
}

inline void print_fp_unwind() {

    TEST_NanoSeconds_Start(nano);

    wechat_backtrace::Frame frames[FRAME_MAX_SIZE];

    uptr frame_size = 0;

    uptr regs[4];
    GetFramePointerMinimalRegs(regs);

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(fp_unwind)(regs, frames, FRAME_MAX_SIZE, frame_size);

    TEST_NanoSeconds_End(wechat_backtrace::fp_unwind, nano, frame_size);

    BENCHMARK_LOGE(FP_UNWIND_TAG, "frames = %"
            PRIuPTR, frame_size);

    for (size_t num = 0; num < frame_size; num++) {
        std::string formatted;
        fp_format_frame(frames[num], num, unwindstack::Regs::CurrentArch() == unwindstack::ARCH_ARM,
                        formatted);

        BENCHMARK_LOGE(FP_UNWIND_TAG, formatted.c_str(), "");
    }
}

inline void print_wechat_quicken_unwind() {

    TEST_NanoSeconds_Start(nano);

    uptr regs[QUT_MINIMAL_REG_SIZE];
    GetQuickenMinimalRegs(regs);
    wechat_backtrace::Frame frames[FRAME_MAX_SIZE];
    uptr frame_size = 0;

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(quicken_unwind)(regs, frames, FRAME_MAX_SIZE,
                                                             frame_size);

    TEST_NanoSeconds_End(print_wechat_quicken_unwind, nano, frame_size);

    LOGE(WECHAT_BACKTRACE_TAG, "frames = %"
            PRIuPTR, frame_size);;

    wechat_backtrace::UpdateLocalMaps();
    std::shared_ptr<unwindstack::LocalMaps> local_maps = wechat_backtrace::GetMapsCache();
    if (!local_maps) {
        BENCHMARK_LOGE(DWARF_UNWIND_TAG, "Err: unable to get maps.");
        return;
    }
    auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
    unwindstack::JitDebug jit_debug(process_memory);
    jit_debug.SetArch(unwindstack::Regs::CurrentArch());

    for (size_t num = 0; num < frame_size; num++) {

        unwindstack::MapInfo *map_info = local_maps->Find(frames[num].pc);
        std::string function_name;
        uint64_t function_offset = 0;
        unwindstack::Elf *elf = nullptr;
        if (map_info) {

            if (frames[num].is_dex_pc) {
                frames[num].rel_pc = frames[num].pc - map_info->start;
            } else {

                elf = map_info->GetElf(process_memory,
                                       unwindstack::Regs::CurrentArch());

                elf->GetFunctionName(frames[num].rel_pc, &function_name, &function_offset);
                if (!elf->valid()) {
                    unwindstack::Elf *jit_elf = jit_debug.GetElf(local_maps.get(),
                                                                 frames[num].rel_pc);
                    if (jit_elf) {
                        jit_elf->GetFunctionName(frames[num].rel_pc, &function_name,
                                                 &function_offset);
                    }
                }
            }
        }

        std::string formatted;
        quicken_format_frame(frames[num], map_info, elf, num,
                             unwindstack::Regs::CurrentArch() == unwindstack::ARCH_ARM,
                             function_name, function_offset, formatted);

        BENCHMARK_LOGE(WECHAT_BACKTRACE_TAG, formatted.c_str(), "");
    }
}

void leaf_func(const char *testcase) {

    BENCHMARK_LOGD(UNWIND_TEST_TAG, "Test %s unwind start with mode %d.", testcase, gMode);

    switch (gMode) {
        case FP_UNWIND:
            print_fp_unwind();
            break;
        case WECHAT_QUICKEN_UNWIND:
            print_wechat_quicken_unwind();
            break;
        case DWARF_UNWIND:
            print_dwarf_unwind();
            break;
        default:
            BENCHMARK_LOGE(UNWIND_TEST_TAG, "Unknown test %s with mode %d.", testcase, gMode);
            break;
    }

    BENCHMARK_LOGD(UNWIND_TEST_TAG, "Test %s unwind finished.", testcase);
}

#ifdef __cplusplus
}
#endif