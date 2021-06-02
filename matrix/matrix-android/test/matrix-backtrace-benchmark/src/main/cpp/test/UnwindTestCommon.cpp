#include <jni.h>
#include <dlfcn.h>
#include <cinttypes>
#include <cxxabi.h>
#include <inttypes.h>
#include <MinimalRegs.h>
#include <QuickenUnwinder.h>
#include <QuickenMaps.h>
#include <LocalMaps.h>
#include <DebugJit.h>
#include "Backtrace.h"
#include "UnwindTestCommon.h"
#include "BenchmarkLog.h"
#include "BacktraceDefine.h"
#include "DebugDexFiles.h"
#include "../../../../../../matrix-backtrace/src/main/cpp/external/libunwindstack/deps/android-base/include/android-base/stringprintf.h"
#include "EHUnwindBacktrace.h"
#include "JavaStacktrace.h"

#define JAVA_UNWIND_TAG "Java-Unwind"
#define DWARF_UNWIND_TAG "Dwarf-Unwind"
#define FP_UNWIND_TAG "Fp-Unwind"
#define WECHAT_BACKTRACE_TAG "WeChat-Quicken-Unwind"

#define TEST_NanoSeconds_Start(timestamp) \
        long timestamp = 0; \
        if (!gBenchmarkWarmUp) { \
            struct timespec tms; \
            if (clock_gettime(CLOCK_MONOTONIC, &tms)) { \
                BENCHMARK_LOGE(UNWIND_TEST_TAG, "Err: Get time failed."); \
            } else { \
                timestamp = tms.tv_nsec; \
            } \
        }

#define TEST_NanoSeconds_End(tag, timestamp, frames) \
        if (!gBenchmarkWarmUp) { \
            struct timespec tms; \
            if (clock_gettime(CLOCK_MONOTONIC, &tms)) { \
                BENCHMARK_LOGE(UNWIND_TEST_TAG, "Err: Get time failed."); \
            } \
            long duration = (tms.tv_nsec - timestamp); \
            if (duration <= 0) { \
                BENCHMARK_LOGE(UNWIND_TEST_TAG, "Err: duration is negative."); \
            } else { \
                BENCHMARK_LOGE(UNWIND_TEST_TAG, #tag" %ld(ns) - %ld(ns) = costs: %ld(ns), frames = %zu" \
                    , tms.tv_nsec, timestamp, duration, (size_t) frames); \
                benchmark_counting(duration, frames); \
            } \
        }


#ifdef __cplusplus
extern "C" {
#endif

static UnwindTestMode gMode = DWARF_UNWIND;

static bool gBenchmarkWarmUp = false;
static bool gPrintStack = false;
static bool gShrinkJavaStack = false;

void set_unwind_mode(UnwindTestMode mode) {
    gMode = mode;
}

uint64_t sTotalDuration = 0;
size_t sBenchmarkTimes = 0;
size_t sLastFrameSize = 0;

void reset_benchmark_counting() {
    sTotalDuration = 0;
    sBenchmarkTimes = 0;
    sLastFrameSize = 0;
}

void benchmark_counting(uint64_t duration, size_t frame_size) {

    sTotalDuration += duration;
    sBenchmarkTimes++;
    sLastFrameSize = frame_size;
}

void dump_benchmark_calculation(const char *tag) {
    BENCHMARK_RESULT_LOGE(UNWIND_TEST_TAG,
                          "%s Accumulated duration = %llu, times = %zu, avg = %llu, frame-size = %zu, per-frame = %llu.",
                          tag, (unsigned long long) sTotalDuration, sBenchmarkTimes,
                          (unsigned long long) (sTotalDuration / sBenchmarkTimes),
                          sLastFrameSize,
                          (unsigned long long) (
                                  ((unsigned long long) (sTotalDuration / sBenchmarkTimes)) /
                                  sLastFrameSize));
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

void fp_format_frame(uptr pc, size_t num, bool is32Bit, std::string &data) {

    Dl_info stack_info;
    dladdr((void *) pc, &stack_info);

    uptr rel_pc = pc - (uptr) stack_info.dli_fbase;
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

void
quicken_simple_format_frame(wechat_backtrace::Frame &frame, size_t num, bool is32Bit,
                            std::string &function_name, uint64_t function_offset,
                            std::string &data) {

    if (is32Bit) {
        data += android::base::StringPrintf("  #%02zu pc %08" PRIx64, num, (uint64_t) frame.rel_pc);
    } else {
        data += android::base::StringPrintf("  #%02zu pc %016" PRIx64, num,
                                            (uint64_t) frame.rel_pc);
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
}

inline void print_java_unwind() {
    TEST_NanoSeconds_Start(nano);
    jobject throwable;
    java_unwind(throwable);
    TEST_NanoSeconds_End(java_unwind, nano, 20);
    // TODO print java stack
}

inline void print_eh_unwind() {

    TEST_NanoSeconds_Start(nano);

    uintptr_t frames[FRAME_MAX_SIZE];
    size_t frame_size = eh_unwind(frames, FRAME_MAX_SIZE);

    TEST_NanoSeconds_End(wechat_backtrace::eh_unwind, nano, 20);

    if (!gPrintStack) {
        return;
    }

    for (size_t num = 0; num < frame_size; num++) {
        std::string formatted;
        fp_format_frame(frames[num], num, unwindstack::Regs::CurrentArch() == unwindstack::ARCH_ARM,
                        formatted);

        BENCHMARK_LOGE("EH_UNWIND", formatted.c_str(), "");
    }
}


inline void print_dwarf_unwind() {

    std::vector<unwindstack::FrameData> frames;

    TEST_NanoSeconds_Start(nano);

    std::shared_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
    unwindstack::RegsGetLocal(regs.get());

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(dwarf_unwind)(regs.get(), frames, FRAME_MAX_SIZE);

    TEST_NanoSeconds_End(unwindstack::dwarf_unwind, nano, frames.size());

    if (!gPrintStack) {
        return;
    }

    wechat_backtrace::UpdateLocalMaps();
    std::shared_ptr<wechat_backtrace::Maps> quicken_maps = wechat_backtrace::Maps::current();
    if (!quicken_maps) {
        BENCHMARK_LOGE(DWARF_UNWIND_TAG, "Err: unable to get maps.");
        return;
    }
    auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
    auto jit_debug = wechat_backtrace::DebugJit::Instance();
    auto dex_debug = wechat_backtrace::DebugDexFiles::Instance();
    size_t num = 0;
    for (auto p_frame = frames.begin(); p_frame != frames.end(); ++p_frame, num++) {

        unwindstack::MapInfo *map_info = quicken_maps->Find(p_frame->pc);
        unwindstack::Elf *elf = nullptr;
        if (map_info) {

            if (p_frame->is_dex_pc) {
                dex_debug->GetMethodInformation(quicken_maps.get(), map_info, p_frame->pc,
                                                &p_frame->function_name,
                                                &p_frame->function_offset);
            } else {

                elf = map_info->GetElf(process_memory, regs->Arch());
                elf->GetFunctionName(p_frame->rel_pc, &p_frame->function_name,
                                     &p_frame->function_offset);
                if (!elf->valid()) {
                    unwindstack::Elf *jit_elf = jit_debug->GetElf(quicken_maps.get(), p_frame->pc);
                    if (jit_elf) {
                        jit_elf->GetFunctionName(p_frame->pc, &p_frame->function_name,
                                                 &p_frame->function_offset);
                    }
                }
            }
        }
        std::string formatted;
        dwarf_format_frame(*p_frame, map_info, elf, regs->Arch() == unwindstack::ARCH_ARM,
                           formatted);

        BENCHMARK_LOGE(DWARF_UNWIND_TAG, formatted.c_str(), "");
    }
}

inline void print_fp_and_java_unwind() {

    TEST_NanoSeconds_Start(nano);

    wechat_backtrace::Frame frames[FRAME_MAX_SIZE];

    uptr frame_size = 0;

    uptr regs[4];
    GetFramePointerMinimalRegs(regs);

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(fp_unwind)(regs, frames, FRAME_MAX_SIZE, frame_size);

    jobject throwable;
    java_unwind(throwable);

    TEST_NanoSeconds_End(fp_and_java_unwind, nano, frame_size);

    if (!gPrintStack) {
        return;
    }

    for (size_t num = 0; num < frame_size; num++) {
        std::string formatted;
        fp_format_frame(frames[num].pc, num,
                        unwindstack::Regs::CurrentArch() == unwindstack::ARCH_ARM,
                        formatted);

        BENCHMARK_LOGE(FP_UNWIND_TAG, formatted.c_str(), "");
    }

    // TODO print java stack
}

inline void print_fp_unwind() {

    TEST_NanoSeconds_Start(nano);

    wechat_backtrace::Frame frames[FRAME_MAX_SIZE];

    uptr frame_size = 0;

    uptr regs[4];
    GetFramePointerMinimalRegs(regs);

    wechat_backtrace::BACKTRACE_FUNC_WRAPPER(fp_unwind)(regs, frames, FRAME_MAX_SIZE, frame_size);

    TEST_NanoSeconds_End(wechat_backtrace::fp_unwind, nano, frame_size);

    if (!gPrintStack) {
        return;
    }

    for (size_t num = 0; num < frame_size; num++) {
        std::string formatted;
        fp_format_frame(frames[num].pc, num,
                        unwindstack::Regs::CurrentArch() == unwindstack::ARCH_ARM,
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

    TEST_NanoSeconds_End(wechat_quicken_unwind, nano, frame_size);

    if (!gPrintStack) {
        return;
    }

    if (gShrinkJavaStack) {
        std::shared_ptr<wechat_backtrace::Maps> quicken_maps = wechat_backtrace::Maps::current();
        auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());
        auto dex_debug = wechat_backtrace::DebugDexFiles::Instance();
        auto jit_debug = wechat_backtrace::DebugJit::Instance();
        bool found_java_frame = false;
        for (size_t num = 0; num < frame_size; num++) {

            if (found_java_frame && !frames[num].maybe_java) continue;

            if (found_java_frame != frames[num].maybe_java) {
                BENCHMARK_LOGE(WECHAT_BACKTRACE_TAG, "Java stacktrace:");
                found_java_frame = frames[num].maybe_java;
            }

            unwindstack::MapInfo *map_info = quicken_maps->Find(frames[num].pc);
            std::string function_name = "";
            uint64_t function_offset = 0;
            unwindstack::Elf *elf = nullptr;
            if (map_info) {

                if (frames[num].is_dex_pc) {
                    frames[num].rel_pc = frames[num].pc - map_info->start;

                    dex_debug->GetMethodInformation(quicken_maps.get(), map_info, frames[num].pc,
                                                    &function_name,
                                                    &function_offset);
                } else {

                    elf = map_info->GetElf(process_memory,
                                           unwindstack::Regs::CurrentArch());

                    elf->GetFunctionName(frames[num].rel_pc, &function_name, &function_offset);
                    if (!elf->valid()) {
                        unwindstack::Elf *jit_elf = jit_debug->GetElf(quicken_maps.get(),
                                                                      frames[num].pc);
                        if (jit_elf) {
                            jit_elf->GetFunctionName(frames[num].pc, &function_name,
                                                     &function_offset);
                        }
                    }
                }
            }

            std::string formatted;
            if (!found_java_frame) {
                quicken_format_frame(
                        frames[num], map_info, elf, num,
                        unwindstack::Regs::CurrentArch() == unwindstack::ARCH_ARM,
                        function_name, function_offset, formatted);
            } else {
                quicken_simple_format_frame(
                        frames[num], num,
                        unwindstack::Regs::CurrentArch() == unwindstack::ARCH_ARM,
                        function_name, function_offset, formatted);
            }

            BENCHMARK_LOGE(WECHAT_BACKTRACE_TAG, formatted.c_str(), "");
        }
    } else {
        wechat_backtrace::UpdateLocalMaps();
        std::shared_ptr<wechat_backtrace::Maps> quicken_maps = wechat_backtrace::Maps::current();
        if (!quicken_maps) {
            BENCHMARK_LOGE(DWARF_UNWIND_TAG, "Err: unable to get maps.");
            return;
        }
        auto process_memory = unwindstack::Memory::CreateProcessMemory(getpid());

        auto dex_debug = wechat_backtrace::DebugDexFiles::Instance();
        auto jit_debug = wechat_backtrace::DebugJit::Instance();
        for (size_t num = 0; num < frame_size; num++) {

            unwindstack::MapInfo *map_info = quicken_maps->Find(frames[num].pc);
            std::string function_name = "";
            uint64_t function_offset = 0;
            unwindstack::Elf *elf = nullptr;
            if (map_info) {

                if (frames[num].is_dex_pc) {
                    frames[num].rel_pc = frames[num].pc - map_info->start;

                    dex_debug->GetMethodInformation(quicken_maps.get(), map_info, frames[num].pc,
                                                    &function_name,
                                                    &function_offset);
                } else {

                    elf = map_info->GetElf(process_memory,
                                           unwindstack::Regs::CurrentArch());

                    elf->GetFunctionName(frames[num].rel_pc, &function_name, &function_offset);
                    if (!elf->valid()) {
                        unwindstack::Elf *jit_elf = jit_debug->GetElf(quicken_maps.get(),
                                                                      frames[num].pc);
                        if (jit_elf) {
                            jit_elf->GetFunctionName(frames[num].pc, &function_name,
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
}

void leaf_func(const char *testcase) {

    BENCHMARK_LOGD(UNWIND_TEST_TAG, "Test %s unwind start with mode %d.", testcase, gMode);

    switch (gMode) {
        case FP_UNWIND:
            print_fp_unwind();
            break;
        case FP_AND_JAVA_UNWIND:
            print_fp_and_java_unwind();
            break;
        case WECHAT_QUICKEN_UNWIND:
            print_wechat_quicken_unwind();
            break;
        case DWARF_UNWIND:
            print_dwarf_unwind();
            break;
        case JAVA_UNWIND:
            print_java_unwind();
            break;
        case COMM_EH_UNWIND:
            print_eh_unwind();
            break;
        default:
            BENCHMARK_LOGE(UNWIND_TEST_TAG, "Unknown test %s with mode %d.", testcase, gMode);
            break;
    }

    BENCHMARK_LOGD(UNWIND_TEST_TAG, "Test %s unwind finished.", testcase);
}

void benchmark_warm_up() {
    gBenchmarkWarmUp = true;
    bool preValue = gPrintStack;
    gPrintStack = false;
    for (size_t i = 0; i < 100; i++) {
        print_fp_unwind();
        print_fp_and_java_unwind();
        print_wechat_quicken_unwind();
        print_dwarf_unwind();
        print_java_unwind();
        print_eh_unwind();
    }
    gBenchmarkWarmUp = false;
    gPrintStack = preValue;
}

#ifdef __cplusplus
}
#endif