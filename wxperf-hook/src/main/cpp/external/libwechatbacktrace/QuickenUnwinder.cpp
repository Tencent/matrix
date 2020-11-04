#include <stdint.h>
#include <bits/pthread_types.h>
#include <cstdlib>
#include <pthread.h>
#include <FpFallbackUnwinder.h>
#include <deps/android-base/include/android-base/strings.h>
#include <FastArmExidx.h>
#include <include/unwindstack/DwarfError.h>

#include "QuickenUnwinder.h"
#include "QuickenMaps.h"


#include "android-base/include/android-base/macros.h"
#include "../../common/Log.h"

#define WECHAT_QUICKEN_UNWIND_TAG "WeChatQuickenUnwind"

namespace wechat_backtrace {

using namespace std;
using namespace unwindstack;

//void GenerateWeChatQuickenUnwindTable() {
//
//    std::shared_ptr<Maps> maps = Maps::current();
//    auto process_memory_ = unwindstack::Memory::CreateProcessMemory(getpid());
//
//    std::vector<MapInfoPtr> map_infos = maps->FindMapInfoByName(std::string("libwxperf-jni.so"));
//
//    for (MapInfoPtr map_info : map_infos) {
//        unwindstack::ArchEnum arch = unwindstack::ArchEnum::ARCH_ARM;
//        unwindstack::Elf *elf = map_info->GetElf(process_memory_, arch);
//
//        QuickenInterface *interface = map_info->GetQuickenInterface(process_memory_, arch);
//        interface->GenerateQuickenTableUltra(process_memory_.get());
//        break;
//    }
//}

inline uint32_t GetPcAdjustment(uint32_t rel_pc, uint32_t load_bias, QuickenInterface* interface) {
    if (rel_pc < load_bias) {
        if (rel_pc < 2) {
            return 0;
        }
        return 2;
    }
    uint32_t adjusted_rel_pc = rel_pc - load_bias;
    if (adjusted_rel_pc < 5) {
        if (adjusted_rel_pc < 2) {
            return 0;
        }
        return 2;
    }

    if (adjusted_rel_pc & 1) {
        // This is a thumb instruction, it could be 2 or 4 bytes.
        uint32_t value;
        if (!interface->Memory()->ReadFully(adjusted_rel_pc - 5, &value, sizeof(value)) ||
            (value & 0xe000f000) != 0xe000f000) {
            return 2;
        }
    }
    return 4;
}

QutErrorCode WeChatQuickenUnwind(ArchEnum arch, uptr* regs, uptr* backtrace, uptr frame_max_size, uptr &frame_size) {

    std::shared_ptr<Maps> maps = Maps::current();

    auto process_memory_ = unwindstack::Memory::CreateProcessMemory(getpid());

    bool adjust_pc = false;
    bool finished = false;

    MapInfoPtr last_map_info = nullptr;
    QuickenInterface* last_interface = nullptr;
    uint64_t last_load_bias = 0;
    uint64_t dex_pc = 0;

    QutErrorCode ret = QUT_ERROR_NONE;

    for (; frame_size < frame_max_size;) {

        uint64_t cur_pc = PC(regs);
        uint64_t cur_sp = SP(regs);
        QUT_DEBUG_LOG("WeChatQuickenUnwind cur_pc:%llx, cur_sp:%llx", cur_pc, cur_sp);
        MapInfoPtr map_info = nullptr;
        QuickenInterface* interface = nullptr;

        if (last_map_info && last_map_info->start <= cur_pc && last_map_info->end > cur_pc) {
            map_info = last_map_info;
            interface = last_interface;
        } else {
            map_info = maps->Find(cur_pc);

            if (map_info == nullptr) {
                backtrace[frame_size] = PC(regs) - 2;
                LOGE(WECHAT_QUICKEN_UNWIND_TAG, "Map info is null.");
                ret = QUT_ERROR_INVALID_MAP;
                break;
            }

            interface = map_info->GetQuickenInterface(process_memory_, arch);
            if (!interface) {
                LOGE(WECHAT_QUICKEN_UNWIND_TAG, "Quicken interface is null, maybe the elf is invalid.");
                backtrace[frame_size] = PC(regs) - 2;
                ret = QUT_ERROR_INVALID_ELF;
                break;
            }

            last_map_info = map_info;
            last_interface = interface;
            last_load_bias = interface->GetLoadBias();
        }

        uint64_t pc_adjustment = 0;
        uint64_t step_pc;
        uint64_t rel_pc;
        step_pc = PC(regs);
        rel_pc = map_info->GetRelPc(step_pc);

        // Everyone except elf data in gdb jit debug maps uses the relative pc.
        if (!(map_info->flags & MAPS_FLAGS_JIT_SYMFILE_MAP)) {
            step_pc = rel_pc;
        }

        if (adjust_pc) {
            pc_adjustment = GetPcAdjustment(last_load_bias, rel_pc, interface);
        } else {
            pc_adjustment = 0;
        }
        step_pc -= pc_adjustment;

        QUT_DEBUG_LOG("WeChatQuickenUnwind pc_adjustment:%llx, step_pc:%llx, rel_pc:%llx", pc_adjustment, step_pc, rel_pc);

        if (dex_pc != 0) {
            LOGE(WECHAT_QUICKEN_UNWIND_TAG, "dex_pc %llx", dex_pc);
            backtrace[frame_size++] = dex_pc;
            dex_pc = 0;
        }

        backtrace[frame_size++] = PC(regs) - pc_adjustment;

        adjust_pc = true;

        if (frame_size >= frame_max_size) {
            ret = QUT_ERROR_MAX_FRAMES_EXCEEDED;
            break;
        }

        if (!interface->Step(step_pc, regs, process_memory_.get(), &dex_pc, &finished)) {
            ret = interface->last_error_code_;
            break;
        }

        if (finished) { // finished.
            break;
        }

        // If the pc and sp didn't change, then consider everything stopped.
        if (cur_pc == PC(regs) && cur_sp == SP(regs)) {
            LOGE(WECHAT_QUICKEN_UNWIND_TAG, "pc and sp not changed.");
            ret = QUT_ERROR_REPEATED_FRAME;
            break;
        }
    }

    return ret;
}

} // namespace wechat_backtrace