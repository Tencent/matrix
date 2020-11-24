#include <stdint.h>
#include <bits/pthread_types.h>
#include <cstdlib>
#include <pthread.h>
#include <deps/android-base/include/android-base/strings.h>
#include <include/unwindstack/DwarfError.h>
#include <dlfcn.h>
#include <cinttypes>
#include <sys/mman.h>
#include <fcntl.h>
#include <deps/android-base/include/android-base/unique_fd.h>
#include <sys/stat.h>
#include <QuickenTableManager.h>
#include <jni.h>
#include <QuickenUtility.h>

#include "QuickenUnwinder.h"
#include "QuickenMaps.h"


#include "android-base/include/android-base/macros.h"
#include "../../common/Log.h"
#include "../../common/PthreadExt.h"

#define WECHAT_QUICKEN_UNWIND_TAG "QuickenUnwind"

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    class MemoryFile : public Memory {
    public:
        MemoryFile() = default;

        virtual ~MemoryFile();

        bool Init(const std::string &file, uint64_t offset, uint64_t size = UINT64_MAX);

        size_t Read(uint64_t addr, void *dst, size_t size) override;

        size_t Size() { return size_; }

        void Clear() override;

    protected:
        size_t size_ = 0;
        size_t offset_ = 0;
        uint8_t *data_ = nullptr;
    };


    MemoryFile::~MemoryFile() {
        Clear();
    }

    void MemoryFile::Clear() {
        if (data_) {
            munmap(&data_[-offset_], size_ + offset_);
            data_ = nullptr;
        }
    }

    bool MemoryFile::Init(const std::string &file, uint64_t offset, uint64_t size) {
        // Clear out any previous data if it exists.
        Clear();

        android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(file.c_str(), O_RDONLY | O_CLOEXEC)));
        if (fd == -1) {
            return false;
        }
        struct stat buf;
        if (fstat(fd, &buf) == -1) {
            return false;
        }
        if (offset >= static_cast<uint64_t>(buf.st_size)) {
            return false;
        }

        offset_ = offset & (getpagesize() - 1);
        uint64_t aligned_offset = offset & ~(getpagesize() - 1);
        if (aligned_offset > static_cast<uint64_t>(buf.st_size) ||
            offset > static_cast<uint64_t>(buf.st_size)) {
            return false;
        }

        size_ = buf.st_size - aligned_offset;
        uint64_t max_size;
        if (!__builtin_add_overflow(size, offset_, &max_size) && max_size < size_) {
            // Truncate the mapped size.
            size_ = max_size;
        }
        void *map = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, aligned_offset);
        if (map == MAP_FAILED) {
            return false;
        }

        data_ = &reinterpret_cast<uint8_t *>(map)[offset_];
        size_ -= offset_;

        return true;
    }

    size_t MemoryFile::Read(uint64_t addr, void *dst, size_t size) {
        if (addr >= size_) {
            return 0;
        }

        size_t bytes_left = size_ - static_cast<size_t>(addr);
        const unsigned char *actual_base = static_cast<const unsigned char *>(data_) + addr;
        size_t actual_len = std::min(bytes_left, size);

        memcpy(dst, actual_base, actual_len);
        return actual_len;
    }

    void StatisticWeChatQuickenUnwindTable(const string &sopath) {

        string soname = SplitSonameFromPath(sopath);

        QUT_STAT_LOG("Statistic sopath %s so %s", sopath, soname.c_str());
        MemoryFile *memory = new MemoryFile();;
        if (!memory->Init(string(sopath), 0)) {
            QUT_STAT_LOG("memory->Init so %s failed", sopath);
            return;
        }

        auto elf = make_unique<Elf>(memory);

        elf->Init();
        if (!elf->valid()) {
            QUT_STAT_LOG("elf->valid() so %s invalid", sopath);
            return;
        }

        auto process_memory_ = unwindstack::Memory::CreateProcessMemory(getpid());

        SetCurrentStatLib(soname);

        QuickenInterface *interface = QuickenMapInfo::GetQuickenInterfaceFromElf(sopath, elf.get());
        interface->GenerateQuickenTable<addr_t>(process_memory_.get());

        DumpQutStatResult();
    }

    void GenerateQutForLibrary(const std::string &sopath) {

        QUT_LOG("Generate qut for so %s.", sopath.c_str());

        const string hash = ToHash(sopath);
        const std::string soname = SplitSonameFromPath(sopath);

        if (QuickenTableManager::CheckIfQutFileExistsWithHash(soname, hash)) {
            QUT_LOG("Qut exists with hash %s and return.", hash.c_str());
            return;
        }

        auto memory = new MemoryFile(); // Will be destructed by 'elf' instance.
        auto elf = make_unique<Elf>(memory);
        if (!memory->Init(string(sopath), 0)) {
            QUT_LOG("memory->Init so %s failed", sopath.c_str());
            return;
        }
        elf->Init();
        if (!elf->valid()) {
            QUT_LOG("elf->valid() so %s invalid", sopath.c_str());
            return;
        }

        const string build_id_hex = elf->GetBuildID();
        const string build_id = ToBuildId(build_id_hex);

        if (QuickenTableManager::CheckIfQutFileExistsWithBuildId(soname, build_id)) {
            QUT_LOG("Qut exists with build id %s and return.", build_id.c_str());
            return;
        }

        auto process_memory_ = unwindstack::Memory::CreateProcessMemory(getpid());

        QuickenInterface *interface = QuickenMapInfo::GetQuickenInterfaceFromElf(sopath, elf.get());

        interface->GenerateQuickenTable<addr_t>(process_memory_.get());

        QutFileError error = QuickenTableManager::getInstance().SaveQutSections(
                soname, sopath, hash, build_id, build_id_hex, interface->GetQutSections());
        QUT_LOG("Generate qut for so %s result %d", sopath.c_str(), error);
    }

    void ConsumeRequestingQut() {
        auto requesting_qut = QuickenTableManager::getInstance().GetRequestQut();
        auto iter = requesting_qut.begin();
        while (iter != requesting_qut.end()) {
            GenerateQutForLibrary(iter->second);
            iter++;
        }
    }

    inline uint32_t
    GetPcAdjustment(uint32_t rel_pc, uint32_t load_bias, QuickenInterface *interface) {
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

    QutErrorCode
    WeChatQuickenUnwind(const ArchEnum arch, uptr *regs, const uptr frame_max_size,
                        Frame *backtrace, uptr &frame_size) {

        std::shared_ptr<Maps> maps = Maps::current();

        auto process_memory_ = unwindstack::Memory::CreateProcessMemory(getpid());

        bool adjust_pc = false;
        bool finished = false;

        MapInfoPtr last_map_info = nullptr;
        QuickenInterface *last_interface = nullptr;
        uint64_t last_load_bias = 0;
        uint64_t dex_pc = 0;

        QutErrorCode ret = QUT_ERROR_NONE;

        for (; frame_size < frame_max_size;) {

            uint64_t cur_pc = PC(regs);
            uint64_t cur_sp = SP(regs);
            QUT_DEBUG_LOG("WeChatQuickenUnwind cur_pc:%llx, cur_sp:%llx", cur_pc, cur_sp);
            MapInfoPtr map_info = nullptr;
            QuickenInterface *interface = nullptr;

            if (last_map_info && last_map_info->start <= cur_pc && last_map_info->end > cur_pc) {
                map_info = last_map_info;
                interface = last_interface;
            } else {
                map_info = maps->Find(cur_pc);

                if (map_info == nullptr) {
                    backtrace[frame_size].pc = PC(regs) - 2;
                    QUT_DEBUG_LOG(WECHAT_QUICKEN_UNWIND_TAG, "Map info is null.");
                    ret = QUT_ERROR_INVALID_MAP;
                    break;
                }

                interface = map_info->GetQuickenInterface(process_memory_, arch);
                if (!interface) {
                    QUT_DEBUG_LOG(WECHAT_QUICKEN_UNWIND_TAG,
                                  "Quicken interface is null, maybe the elf is invalid.");
                    backtrace[frame_size].pc = PC(regs) - 2;
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

            QUT_DEBUG_LOG("WeChatQuickenUnwind pc_adjustment:%llx, step_pc:%llx, rel_pc:%llx",
                          pc_adjustment, step_pc, rel_pc);
            QUT_TMP_LOG("WeChatQuickenUnwind pc_adjustment:%llx, step_pc:%llx, rel_pc:%llx",
                        pc_adjustment, step_pc, rel_pc);

            if (dex_pc != 0) {
                QUT_DEBUG_LOG(WECHAT_QUICKEN_UNWIND_TAG, "dex_pc %llx", dex_pc);
                backtrace[frame_size++].pc = dex_pc;
//                backtrace[frame_size++].is_dex_pc = true;
                dex_pc = 0;
            }

            backtrace[frame_size++].pc = PC(regs) - pc_adjustment;

            adjust_pc = true;

            if (frame_size >= frame_max_size) {
                ret = QUT_ERROR_MAX_FRAMES_EXCEEDED;
                break;
            }

            if (!interface->Step(step_pc, regs, process_memory_.get(), &dex_pc, &finished)) {
                ret = interface->last_error_code_;
                break;
            }

            QUT_TMP_LOG(
                    "WeChatQuickenUnwind Stepped Reg. r4: %x, r7: %x, r10: %x, r11: %x, sp: %x, ls: %x, pc: %x",
                    R4(regs), R7(regs), R10(regs), R11(regs), SP(regs), LR(regs), PC(regs)
            );

            if (finished) { // finished.
                break;
            }

            // If the pc and sp didn't change, then consider everything stopped.
            if (cur_pc == PC(regs) && cur_sp == SP(regs)) {
                QUT_DEBUG_LOG(WECHAT_QUICKEN_UNWIND_TAG, "pc and sp not changed.");
                ret = QUT_ERROR_REPEATED_FRAME;
                break;
            }
        }

        return ret;
    }


// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------

// Check if given pointer points into allocated stack area.
    static inline bool IsValidFrame(uptr frame, uptr stack_top, uptr stack_bottom) {
        return frame > stack_bottom && frame < stack_top - 2 * sizeof(uptr);
    }

// In GCC on ARM bp points to saved lr, not fp, so we should check the next
// cell in stack to be a saved frame pointer. GetCanonicFrame returns the
// pointer to saved frame pointer in any case.
    static inline uptr *GetCanonicFrame(uptr fp, uptr stack_top, uptr stack_bottom) {
        if (UNLIKELY(stack_top < stack_bottom)) {
            return 0;
        }
//#ifdef __arm__
//    if (!IsValidFrame(fp, stack_top, stack_bottom)) return 0;
//
//    uptr *fp_prev = (uptr *)fp;
//
//    if (IsValidFrame((uptr)fp_prev[0], stack_top, stack_bottom)) return fp_prev;
//
//    // The next frame pointer does not look right. This could be a GCC frame, step
//    // back by 1 word and try again.
//    if (IsValidFrame((uptr)fp_prev[-1], stack_top, stack_bottom)) return fp_prev - 1;
//
//    // Nope, this does not look right either. This means the frame after next does
//    // not have a valid frame pointer, but we can still extract the caller PC.
//    // Unfortunately, there is no way to decide between GCC and LLVM frame
//    // layouts. Assume LLVM.
//    return fp_prev;
//#else
        return (uptr *) fp;
//#endif
    }

    static inline uptr GetPageSize() {
        return 4096;
    }

    static inline bool IsAligned(uptr a, uptr alignment) {
        return (a & (alignment - 1)) == 0;
    }

    static inline void FpUnwind(uptr pc, uptr fp, uptr stack_top, uptr stack_bottom,
                                uptr *backtrace, uptr *frame_pointer, uptr frame_max_size,
                                uptr &frame_size) {

        const uptr kPageSize = GetPageSize();
        backtrace[0] = pc;
        frame_pointer[0] = fp;
        frame_size = 1;
        if (UNLIKELY(stack_top < 4096)) return;  // Sanity check for stack top.
        uptr *frame = GetCanonicFrame(fp, stack_top, stack_bottom);
        // Lowest possible address that makes sense as the next frame pointer.
        // Goes up as we walk the stack.
        uptr bottom = stack_bottom;
        // Avoid infinite loop when frame == frame[0] by using frame > prev_frame.
        while (IsValidFrame((uptr) frame, stack_top, bottom) &&
               IsAligned((uptr) frame, sizeof(*frame)) &&
               frame_size < frame_max_size) {

            uptr pc1 = frame[1];
            // Let's assume that any pointer in the 0th page (i.e. <0x1000 on i386 and
            // x86_64) is invalid and stop unwinding here.  If we're adding support for
            // a platform where this isn't true, we need to reconsider this check.
            if (pc1 < kPageSize)
                break;
            if (pc1 != pc) {
                backtrace[frame_size] = (uptr) pc1;
                frame_pointer[frame_size] = frame[0];
                frame_size++;
            }
            bottom = (uptr) frame;
            frame = GetCanonicFrame((uptr) frame[0], stack_top, bottom);
        }
    }

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

} // namespace wechat_backtrace