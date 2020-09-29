#include <stdint.h>
#include <bits/pthread_types.h>
#include <cstdlib>
#include <pthread.h>
#include <FallbackUnwinder.h>
#include <MapsControll.h>
#include <deps/android-base/include/android-base/strings.h>
#include <FastArmExidx.h>

#include "FastArmExidxUnwinder.h"
#include "QuickenMaps.h"

#include "android-base/include/android-base/macros.h"
#include "../../common/Log.h"

#define FAST_ARM_EXIDX_UNWINDER_TAG "FastExidxUnwind"

namespace wechat_backtrace {

    inline int GetPcAdjustment(uint32_t rel_pc, uint32_t load_bias, unwindstack::Elf* elf) {
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
            if (!elf->memory()->ReadFully(adjusted_rel_pc - 5, &value, sizeof(value)) ||
                (value & 0xe000f000) != 0xe000f000) {
                return 2;
            }
        }
        return 4;
    }

    void FastExidxUnwind(uptr * regs, uptr * backtrace, uptr frame_max_size, uptr &frame_size) {

        std::shared_ptr<Maps> maps = Maps::current();

        unwindstack::SetFastFlag(true);
        auto process_memory_ = unwindstack::Memory::CreateProcessMemory(getpid());

        bool adjust_pc = false;
        MapInfoPtr last_map_info = nullptr;

        for (; frame_size < frame_max_size;) {
            uint64_t cur_pc = PC(regs);
            uint64_t cur_sp = SP(regs);

            MapInfoPtr map_info = nullptr;

            if (last_map_info && last_map_info->start <= cur_pc && last_map_info->end > cur_pc) {
                map_info = last_map_info;
            } else {
                map_info = maps->Find(cur_pc);
                last_map_info = map_info;
            }

            uint64_t pc_adjustment = 0;
            uint64_t step_pc;
            uint64_t rel_pc;
            unwindstack::Elf* elf;
            unwindstack::ArchEnum arch = unwindstack::ArchEnum::ARCH_ARM;
            if (map_info == nullptr) {
                // TODO stop unwind
                LOGE(FAST_ARM_EXIDX_UNWINDER_TAG, "map_info is null!");
                break;
            }

            elf = map_info->GetElf(process_memory_, arch);

            unwindstack::FastArmExidxInterface* interface =
                    map_info->GetFastArmExidxInterface(process_memory_, arch);
            step_pc = PC(regs);
            rel_pc = elf->GetRelPc(step_pc, map_info);

            // Everyone except elf data in gdb jit debug maps uses the relative pc.
            if (!(map_info->flags & MAPS_FLAGS_JIT_SYMFILE_MAP)) {
                step_pc = rel_pc;
            }
            if (adjust_pc) {
                pc_adjustment = GetPcAdjustment(rel_pc, elf, arch);
            } else {
                pc_adjustment = 0;
            }
            step_pc -= pc_adjustment;

            backtrace[frame_size] = PC(regs) - pc_adjustment;
            frame_size++;

            adjust_pc = true;

            bool stepped = false;
            bool finished = false;
            if (map_info != nullptr) {
                if (map_info->flags & MAPS_FLAGS_DEVICE_MAP) {
                    // TODO stop unwind
                    LOGE(FAST_ARM_EXIDX_UNWINDER_TAG, "map_info flags is MAPS_FLAGS_DEVICE_MAP!");
                    break;
                } else {
                    stepped = interface->Step(step_pc, regs, process_memory_.get(), &finished);
                    if (!stepped) {
                        // TODO stop unwind
                        LOGE(FAST_ARM_EXIDX_UNWINDER_TAG, "stepped failed");
                        break;
                    }
                }
            }

            if (finished) {
                // TODO stop unwind
                LOGE(FAST_ARM_EXIDX_UNWINDER_TAG, "finished");
                break;
            }

            // If the pc and sp didn't change, then consider everything stopped.
            if (cur_pc == PC(regs) && cur_sp == SP(regs)) {
                // TODO stop unwind
                LOGE(FAST_ARM_EXIDX_UNWINDER_TAG, "pc and sp not changed.");
                break;
            }
        }
    }



} // namespace wechat_backtrace