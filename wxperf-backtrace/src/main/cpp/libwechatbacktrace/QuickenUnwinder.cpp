#include <stdint.h>
#include <bits/pthread_types.h>
#include <cstdlib>
#include <pthread.h>
#include <android-base/strings.h>
#include <unwindstack/DwarfError.h>
#include <dlfcn.h>
#include <cinttypes>
#include <sys/mman.h>
#include <fcntl.h>
#include <android-base/unique_fd.h>
#include <sys/stat.h>
#include <QuickenTableManager.h>
#include <jni.h>
#include <QuickenUtility.h>
#include <QuickenMemory.h>

#include "QuickenUnwinder.h"
#include "QuickenMaps.h"

#include "android-base/macros.h"
#include "Log.h"
#include "PthreadExt.h"

#define WECHAT_BACKTRACE_TAG "WeChatBacktrace"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    QUT_EXTERN_C_BLOCK

    DEFINE_STATIC_LOCAL(shared_ptr<Memory>, process_memory_, (new QuickenMemoryLocal));

    DEFINE_STATIC_LOCAL(mutex, generate_lock_,);

    void StatisticWeChatQuickenUnwindTable(const string &sopath) {

        string soname = SplitSonameFromPath(sopath);

        QUT_STAT_LOG("Statistic sopath %s so %s", sopath, soname.c_str());
        auto memory = QuickenMapInfo::CreateQuickenMemoryFromFile(sopath, 0);
        if (memory == nullptr) {
            QUT_STAT_LOG("memory->Init so %s failed", sopath);
            return;
        }

        auto elf = make_unique<Elf>(memory);

        elf->Init();
        if (!elf->valid()) {
            QUT_STAT_LOG("elf->valid() so %s invalid", sopath);
            return;
        }

        SetCurrentStatLib(soname);

        unique_ptr<QuickenInterface> interface = QuickenMapInfo::CreateQuickenInterfaceForGenerate(
                sopath,
                elf.get(),
                0   // TODO fix this bug here
        );
        QutSections qut_sections;

        Memory *gnu_debug_data_memory = nullptr;
        if (elf->gnu_debugdata_interface()) {
            gnu_debug_data_memory = elf->gnu_debugdata_interface()->memory();
        }

        interface->GenerateQuickenTable<addr_t>(elf->memory(), gnu_debug_data_memory,
                                                process_memory_.get(),
                                                &qut_sections);

        DumpQutStatResult();
    }

    void NotifyWarmedUpQut(const std::string &sopath, const uint64_t elf_start_offset) {

        const string hash = ToHash(
                sopath + to_string(FileSize(sopath)) + to_string(elf_start_offset));
        const std::string soname = SplitSonameFromPath(sopath);

        QUT_LOG("Notify qut for so %s, elf_start_offset %llu, hash %s.", sopath.c_str(),
                (ullint_t) elf_start_offset, hash.c_str());

        {
            lock_guard<mutex> lock(generate_lock_);

            if (!QuickenTableManager::CheckIfQutFileExistsWithHash(soname, hash)) {
                QUT_LOG("False warmed-up: %s %s", soname.c_str(), hash.c_str());
                return;
            }

            QuickenTableManager::getInstance().EraseQutRequestingByHash(hash);
        }
    }

    bool TestLoadQut(const std::string &so_path, const uint64_t elf_start_offset) {

        QUT_LOG("Try load Qut for so %s, elf_start_offset %llu.", so_path.c_str(),
                (ullint_t) elf_start_offset);

        const string hash = ToHash(
                so_path + to_string(FileSize(so_path)) + to_string(elf_start_offset));
        const std::string so_name = SplitSonameFromPath(so_path);

        {
            lock_guard<mutex> lock(generate_lock_);

            if (!QuickenTableManager::CheckIfQutFileExistsWithHash(so_name, hash)) {
                QUT_LOG("Try load qut, but not exists with hash %s.", hash.c_str());
                return false;
            }

            // Will be destructed by 'elf' instance.
            auto memory = QuickenMapInfo::CreateQuickenMemoryFromFile(so_path, elf_start_offset);
            if (memory == nullptr) {
                QUT_LOG("Try load qut, create quicken memory for so %s failed", so_path.c_str());
                return false;
            }
            auto elf = make_unique<Elf>(memory);
            elf->Init();
            if (!elf->valid()) {
                QUT_LOG("Try load qut, elf->valid() so %s invalid", so_path.c_str());
                return false;
            }

            if (elf->arch() != CURRENT_ARCH) {
                QUT_LOG("Try load qut, elf->arch() invalid %s", so_path.c_str());
                return false;
            }

            const string build_id_hex = elf->GetBuildID();
            const string build_id = build_id_hex.empty() ? FakeBuildId(so_path) : ToBuildId(
                    build_id_hex);

            if (!QuickenTableManager::CheckIfQutFileExistsWithBuildId(so_name, build_id)) {
                QUT_LOG("Try load qut, but not exists with build id %s and return.",
                        build_id.c_str());
                return false;
            }

            QutSectionsPtr qut_sections_tmp = nullptr;  // Test only
            QutFileError ret = QuickenTableManager::getInstance().TryLoadQutFile(
                    so_name, so_path, hash, build_id, qut_sections_tmp, true);

            QUT_LOG("Try load qut for so %s, hash %s, build id %s, result %llu",
                    so_path.c_str(), hash.c_str(), build_id.c_str(),
                    (ullint_t) ret);

            return ret == NoneError;
        }
    }

    bool GenerateQutForLibrary(const std::string &sopath, const uint64_t elf_start_offset,
                               const bool only_save_file) {

        QUT_LOG("Generate qut for so %s, elf_start_offset %llu.", sopath.c_str(),
                (ullint_t) elf_start_offset);

        const string hash = ToHash(
                sopath + to_string(FileSize(sopath)) + to_string(elf_start_offset));
        const std::string soname = SplitSonameFromPath(sopath);

        {
            lock_guard<mutex> lock(generate_lock_);

            if (QuickenTableManager::CheckIfQutFileExistsWithHash(soname, hash)) {
                QUT_LOG("Qut exists with hash %s and return.", hash.c_str());
                return true;
            }

            // Will be destructed by 'elf' instance.
            auto memory = QuickenMapInfo::CreateQuickenMemoryFromFile(sopath, elf_start_offset);
            if (memory == nullptr) {
                QUT_LOG("Create quicken memory for so %s failed", sopath.c_str());
                return false;
            }
            auto elf = make_unique<Elf>(memory);
            elf->Init();
            if (!elf->valid()) {
                QUT_LOG("elf->valid() so %s invalid", sopath.c_str());
                return false;
            }

            if (elf->arch() != CURRENT_ARCH) {
                QUT_LOG("elf->arch() invalid %s", sopath.c_str());
                return false;
            }

            const string build_id_hex = elf->GetBuildID();
            const string build_id = build_id_hex.empty() ? FakeBuildId(sopath) : ToBuildId(
                    build_id_hex);

            if (QuickenTableManager::CheckIfQutFileExistsWithBuildId(soname, build_id)) {
                QUT_LOG("Qut exists with build id %s and return.", build_id.c_str());
                return true;
            }

            unique_ptr<QuickenInterface> interface =
                    QuickenMapInfo::CreateQuickenInterfaceForGenerate(sopath, elf.get(),
                                                                      elf_start_offset);

            std::unique_ptr<QutSections> qut_sections = make_unique<QutSections>();
            QutSectionsPtr qut_sections_ptr = qut_sections.get();

            Memory *gnu_debug_data_memory = nullptr;
            if (elf->gnu_debugdata_interface()) {
                gnu_debug_data_memory = elf->gnu_debugdata_interface()->memory();
            }

            bool ret = interface->GenerateQuickenTable<addr_t>(elf->memory(), gnu_debug_data_memory,
                                                               process_memory_.get(),
                                                               qut_sections_ptr);
            if (ret) {
                QutFileError error = QuickenTableManager::getInstance().SaveQutSections(
                        soname, sopath, hash, build_id, only_save_file, std::move(qut_sections));

                (void) error;

                QUT_LOG("Save qut for so %s result %d", sopath.c_str(), error);
            }

            QUT_LOG("Generate qut for so %s result %d", sopath.c_str(), ret);

            return ret;
        }
    }

    vector<string> ConsumeRequestingQut() {
        auto requesting_qut = QuickenTableManager::getInstance().GetRequestQut();
        auto it = requesting_qut.begin();
        vector<string> consumed;
        while (it != requesting_qut.end()) {
            consumed.push_back(it->second.second + ":" + to_string(it->second.first));
//            GenerateQutForLibrary(it->second.second, it->second.first);
            it++;
        }

        return consumed;
    }

    inline uint32_t
    GetPcAdjustment(Memory *process_memory, MapInfoPtr map_info, uint64_t pc, uint32_t rel_pc,
                    uint32_t load_bias) {
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

        if (pc & 1) {
            // This is a thumb instruction, it could be 2 or 4 bytes.
            uint32_t value;
            uint64_t adjusted_pc = pc - 5;
            if (!(map_info->flags & PROT_READ) ||
                adjusted_pc < map_info->start ||
                (adjusted_pc + sizeof(value)) >= map_info->end ||
                !process_memory->ReadFully(adjusted_pc, &value, sizeof(value)) ||
                (value & 0xe000f000) != 0xe000f000) {
                return 2;
            }
        }
        return 4;
    }


    QutErrorCode
    WeChatQuickenUnwind(const ArchEnum arch, uptr *regs, const size_t frame_max_size,
                        Frame *backtrace, uptr &frame_size) {

        std::shared_ptr<Maps> maps = Maps::current();

        if (!maps) {
            QUT_LOG("Maps is null.");
            return QUT_ERROR_MAPS_IS_NULL;
        }

        bool adjust_pc = false;
        bool finished = false;

        MapInfoPtr last_map_info = nullptr;
        QuickenInterface *last_interface = nullptr;
        uint64_t last_load_bias = 0;
        uint64_t dex_pc = 0;

        QutErrorCode ret = QUT_ERROR_NONE;

        pthread_attr_t attr;
        BACKTRACE_FUNC_WRAPPER(pthread_getattr_ext)(pthread_self(), &attr);
        uptr stack_bottom = reinterpret_cast<uptr>(attr.stack_base);
        uptr stack_top = reinterpret_cast<uptr>(attr.stack_base) + attr.stack_size;

        for (; frame_size < frame_max_size;) {

            uint64_t cur_pc = PC(regs);
            uint64_t cur_sp = SP(regs);
            MapInfoPtr map_info = nullptr;
            QuickenInterface *interface = nullptr;

            if (last_map_info && last_map_info->start <= cur_pc && last_map_info->end > cur_pc) {
                map_info = last_map_info;
                interface = last_interface;
            } else {
                map_info = maps->Find(cur_pc);

                if (map_info == nullptr) {
                    backtrace[frame_size++].pc = PC(regs) - 2;
                    ret = QUT_ERROR_INVALID_MAP;
                    break;
                }
                interface = map_info->GetQuickenInterface(process_memory_, arch);
                if (!interface) {
                    backtrace[frame_size++].pc = PC(regs) - 2;
                    ret = QUT_ERROR_INVALID_ELF;
                    break;
                }

                last_map_info = map_info;
                last_interface = interface;
                last_load_bias = interface->GetLoadBias();
            }

            uint64_t pc_adjustment = 0;
            uint64_t rel_pc = map_info->GetRelPc(PC(regs));;
            uint64_t step_pc = rel_pc;

            if (adjust_pc) {
                pc_adjustment = GetPcAdjustment(process_memory_.get(), map_info, PC(regs), rel_pc,
                                                last_load_bias);
            } else {
                pc_adjustment = 0;
            }

            step_pc -= pc_adjustment;

            if (dex_pc != 0) {
                backtrace[frame_size].is_dex_pc = true;
                backtrace[frame_size].pc = dex_pc;
                dex_pc = 0;

                frame_size++;
                if (frame_size >= frame_max_size) {
                    ret = QUT_ERROR_MAX_FRAMES_EXCEEDED;
                    break;
                }
            }

            backtrace[frame_size].pc = PC(regs) - pc_adjustment;

            adjust_pc = true;

            frame_size++;
            if (frame_size >= frame_max_size) {
                ret = QUT_ERROR_MAX_FRAMES_EXCEEDED;
                break;
            }

            if (!interface->Step(step_pc, regs, nullptr, stack_top, stack_bottom,
                                 frame_size, &dex_pc, &finished)) {
                ret = interface->last_error_code_;
                break;
            }

            if (finished) { // finished.
                break;
            }

            // If the pc and sp didn't change, then consider everything stopped.
            if (cur_pc == PC(regs) && cur_sp == SP(regs)) {
                ret = QUT_ERROR_REPEATED_FRAME;
                break;
            }
        }

        return ret;
    }


// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------

//// Check if given pointer points into allocated stack area.
//    static inline bool IsValidFrame(uptr frame, uptr stack_top, uptr stack_bottom) {
//        return frame > stack_bottom && frame < stack_top - 2 * sizeof(uptr);
//    }
//
//// In GCC on ARM bp points to saved lr, not fp, so we should check the next
//// cell in stack to be a saved frame pointer. GetCanonicFrame returns the
//// pointer to saved frame pointer in any case.
//    static inline uptr *GetCanonicFrame(uptr fp, uptr stack_top, uptr stack_bottom) {
//        if (UNLIKELY(stack_top < stack_bottom)) {
//            return 0;
//        }
////#ifdef __arm__
////    if (!IsValidFrame(fp, stack_top, stack_bottom)) return 0;
////
////    uptr *fp_prev = (uptr *)fp;
////
////    if (IsValidFrame((uptr)fp_prev[0], stack_top, stack_bottom)) return fp_prev;
////
////    // The next frame pointer does not look right. This could be a GCC frame, step
////    // back by 1 word and try again.
////    if (IsValidFrame((uptr)fp_prev[-1], stack_top, stack_bottom)) return fp_prev - 1;
////
////    // Nope, this does not look right either. This means the frame after next does
////    // not have a valid frame pointer, but we can still extract the caller PC.
////    // Unfortunately, there is no way to decide between GCC and LLVM frame
////    // layouts. Assume LLVM.
////    return fp_prev;
////#else
//        return (uptr *) fp;
////#endif
//    }
//
//    static inline uptr GetPageSize() {
//        return 4096;
//    }
//
//    static inline bool IsAligned(uptr a, uptr alignment) {
//        return (a & (alignment - 1)) == 0;
//    }
//
//    static inline void FpUnwind(uptr pc, uptr fp, uptr stack_top, uptr stack_bottom,
//                                uptr *backtrace, uptr *frame_pointer, uptr frame_max_size,
//                                uptr &frame_size) {
//
//        const uptr kPageSize = GetPageSize();
//        backtrace[0] = pc;
//        frame_pointer[0] = fp;
//        frame_size = 1;
//        if (UNLIKELY(stack_top < 4096)) return;  // Sanity check for stack top.
//        uptr *frame = GetCanonicFrame(fp, stack_top, stack_bottom);
//        // Lowest possible address that makes sense as the next frame pointer.
//        // Goes up as we walk the stack.
//        uptr bottom = stack_bottom;
//        // Avoid infinite loop when frame == frame[0] by using frame > prev_frame.
//        while (IsValidFrame((uptr) frame, stack_top, bottom) &&
//               IsAligned((uptr) frame, sizeof(*frame)) &&
//               frame_size < frame_max_size) {
//
//            uptr pc1 = frame[1];
//            // Let's assume that any pointer in the 0th page (i.e. <0x1000 on i386 and
//            // x86_64) is invalid and stop unwinding here.  If we're adding support for
//            // a platform where this isn't true, we need to reconsider this check.
//            if (pc1 < kPageSize)
//                break;
//            if (pc1 != pc) {
//                backtrace[frame_size] = (uptr) pc1;
//                frame_pointer[frame_size] = frame[0];
//                frame_size++;
//            }
//            bottom = (uptr) frame;
//            frame = GetCanonicFrame((uptr) frame[0], stack_top, bottom);
//        }
//    }

//    QutErrorCode
//    WeChatQuickenUnwindV2_WIP(ArchEnum arch, uptr *regs, uptr *backtrace, uptr frame_max_size,
//                              uptr &frame_size) {
//
//        QutErrorCode ret = QUT_ERROR_NONE;
//
//        pthread_attr_t attr;
//        pthread_getattr_ext(pthread_self(), &attr);
//        uptr stack_bottom = reinterpret_cast<uptr>(attr.stack_base);
//        uptr stack_top = reinterpret_cast<uptr>(attr.stack_base) + attr.stack_size;
//
//        uptr sp = SP(regs);
//        uptr fp = R7(regs); // TODO how to guess
//        uptr pc = PC(regs);
//
//        uptr frame_pointer[frame_max_size];
//
//        QUT_TMP_LOG("WeChatQuickenSmartUnwind start pc:%llx, fp:%llx", (uint64_t) pc,
//                    (uint64_t) fp);
//
//        FpUnwind(pc, fp, stack_top, stack_bottom, backtrace, frame_pointer, frame_max_size,
//                 frame_size);
//
//        if (frame_size >= frame_max_size) {
//            return ret;
//        }
//
//        // restore registers
//        std::shared_ptr<Maps> maps = Maps::current();
//        auto process_memory_ = unwindstack::Memory::CreateProcessMemory(getpid());
//
//        MapInfoPtr last_map_info = nullptr;
//        QuickenInterface *last_interface = nullptr;
//        uint64_t last_load_bias = 0;
//
//        uint8_t regs_bits = 0;
//
//        for (size_t i = frame_size - 2; i > 0; i--) {
//            uptr cur_pc = backtrace[i];
//            uptr cur_fp = frame_pointer[i];
//            uptr cur_sp = frame_pointer[i - 1];
//            SP(regs) = cur_sp;
//            PC(regs) = cur_pc;
//
//            MapInfoPtr map_info = nullptr;
//            QuickenInterface *interface = nullptr;
//
//            if (last_map_info && last_map_info->start <= cur_pc && last_map_info->end > cur_pc) {
//                map_info = last_map_info;
//                interface = last_interface;
//            } else {
//                map_info = maps->Find(cur_pc);
//
//                if (map_info == nullptr) {
//                    // maybe pc is invalid
//                    continue;
//                }
//
//                interface = map_info->GetQuickenInterface(process_memory_, arch);
//                if (!interface) {
//                    // maybe pc is invalid
//                    continue;
//                }
//
//                last_map_info = map_info;
//                last_interface = interface;
//                last_load_bias = interface->GetLoadBias();
//            }
//
//            uint64_t pc_adjustment = 0;
//            uint64_t step_pc;
//            uint64_t rel_pc;
//            step_pc = backtrace[i];
//            rel_pc = map_info->GetRelPc(step_pc);
//
//            // Everyone except elf data in gdb jit debug maps uses the relative pc.
//            if (!(map_info->flags & MAPS_FLAGS_JIT_SYMFILE_MAP)) {
//                step_pc = rel_pc;
//            }
//
//            pc_adjustment = GetPcAdjustment(last_load_bias, rel_pc, interface);
//            step_pc -= pc_adjustment;
//
////        QUT_DEBUG_LOG("WeChatQuickenUnwind pc_adjustment:%llx, step_pc:%llx, rel_pc:%llx", pc_adjustment, step_pc, rel_pc);
//            bool found = false;
//            if (!interface->StepBack(step_pc, cur_sp, cur_fp, regs_bits, regs,
//                                     process_memory_.get(), &found)) {
//                ret = interface->last_error_code_;
//                QUT_TMP_LOG(
//                        "WeChatQuickenSmartUnwind failed. r4: %x, r7: %x, r10: %x, r11: %x, sp: %x, ls: %x, pc: %x",
//                        R4(regs), R7(regs), R10(regs), R11(regs), SP(regs), LR(regs), PC(regs)
//                );
//                break;
//            }
//
//            if (found) {
//                break;
//            }
//        }
//
//        QUT_TMP_LOG(
//                "WeChatQuickenSmartUnwind Reg restore end. r4: %x, r7: %x, r10: %x, r11: %x, sp: %x, ls: %x, pc: %x",
//                R4(regs), R7(regs), R10(regs), R11(regs), SP(regs), LR(regs), PC(regs)
//        );
//
//        return ret;
//    }

    QUT_EXTERN_C_BLOCK_END
} // namespace wechat_backtrace