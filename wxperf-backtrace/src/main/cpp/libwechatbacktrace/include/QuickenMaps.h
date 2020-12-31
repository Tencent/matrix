#ifndef _LIBWECHATBACKTRACE_QUICKEN_MAPS_H
#define _LIBWECHATBACKTRACE_QUICKEN_MAPS_H

#include <unwindstack/MapInfo.h>
#include <unordered_map>
#include "QuickenInterface.h"
#include "QuickenMemory.h"

namespace wechat_backtrace {

    // Special flag to indicate a map is in /dev/. However, a map in
    // /dev/ashmem/... does not set this flag.
    static constexpr int MAPS_FLAGS_DEVICE_MAP = 0x8000;

    // Special flag to indicate that this map represents an elf file
    // created by ART for use with the gdb jit debug interface.
    // This should only ever appear in offline maps data.
    static constexpr int MAPS_FLAGS_JIT_SYMFILE_MAP = 0x4000;

    class QuickenInterface;

    typedef std::unordered_map<std::string, std::shared_ptr<QuickenInterface>> interface_caches_t;

    class QuickenMapInfo : public unwindstack::MapInfo {

    public:
        QuickenMapInfo(MapInfo *prevMap, MapInfo *prevRealMap, uint64_t start, uint64_t end,
                       uint64_t offset, uint64_t flags, const char *name) :
                MapInfo(prevMap, prevRealMap, start, end, offset, flags, name) {};

        QuickenInterface *
        GetQuickenInterface(
                std::shared_ptr<unwindstack::Memory> &process_memory,
                unwindstack::ArchEnum expected_arch
        );

        static QuickenInterface *
        CreateQuickenInterfaceFromElf(
                unwindstack::ArchEnum expected_arch,
                const std::string &so_path,
                const std::string &so_name,
                const uint64_t load_bias_,
                const uint64_t elf_offset,
                const uint64_t elf_start_offset,
                const std::string &build_id_hex
        );

        static std::unique_ptr<QuickenInterface>
        CreateQuickenInterfaceForGenerate(
                const std::string &sopath,
                unwindstack::Elf *elf
        );

        static std::unique_ptr<unwindstack::Elf>
        CreateElf(
                const std::string &so_path,
                unwindstack::Memory *memory,
                unwindstack::ArchEnum expected_arch
        );

        static unwindstack::Memory *
        CreateQuickenMemoryFromFile(
                const std::string &so_path,
                const uint64_t elf_start_offset
        );

        uint64_t GetRelPc(uint64_t pc);

        std::shared_ptr<QuickenInterface> quicken_interface_;

        bool quicken_interface_failed_ = false;

        uint64_t elf_load_bias_;

    protected:

        unwindstack::Memory *GetFileQuickenMemory();

        bool InitFileMemoryFromPreviousReadOnlyMap(QuickenMemoryFile *memory);

        unwindstack::Memory *
        CreateQuickenMemory(const std::shared_ptr<unwindstack::Memory> &process_memory);

        static std::mutex &lock_;

        static interface_caches_t &cached_quicken_interface_;
    };

    typedef QuickenMapInfo *MapInfoPtr;

    class Maps {

    public:
        Maps() {};

        Maps(size_t start_capacity) : maps_capacity_(start_capacity) {};

        ~Maps() {
            if (local_maps_ != nullptr) {
                ReleaseLocalMaps();
            }
        };

        // Maps are not copyable but movable, because they own pointers to MapInfo
        // objects.
        Maps(const Maps &) = delete;

        Maps &operator=(const Maps &) = delete;

        Maps(Maps &&) = default;

        Maps &operator=(Maps &&) = default;

        size_t GetSize();

        MapInfoPtr Find(uint64_t pc);

        std::vector<MapInfoPtr> FindMapInfoByName(std::string soname);

        static bool Parse();

        static std::shared_ptr<Maps> current();

        MapInfoPtr *local_maps_ = nullptr;
        size_t maps_capacity_ = 0;
        size_t maps_size_ = 0;

    protected:

        void ReleaseLocalMaps();

        static std::mutex &maps_lock_;
        static std::shared_ptr<Maps>& current_maps_;
        static size_t latest_maps_capacity_;

    private:
        bool ParseImpl();

    };

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_MAPS_H
