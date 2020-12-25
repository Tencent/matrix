#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/unique_fd.h>
#include <procinfo/process_map.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>
#include <LocalMaps.h>
#include <QuickenMemory.h>
#include <MemoryRange.h>

#include "QuickenUtility.h"
#include "ElfInterfaceArm.h"
#include "QuickenMaps.h"

#define CAPACITY_INCREMENT 1024

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    DEFINE_STATIC_CPP_FIELD(mutex, Maps::maps_lock_,);
    DEFINE_STATIC_CPP_FIELD(shared_ptr<Maps>, Maps::current_maps_,);
    size_t Maps::latest_maps_capacity_ = CAPACITY_INCREMENT;

    DEFINE_STATIC_CPP_FIELD(mutex, QuickenMapInfo::lock_,);
    DEFINE_STATIC_CPP_FIELD(interface_caches_t, QuickenMapInfo::cached_quicken_interface_,);

    QuickenInterface *
    QuickenMapInfo::GetQuickenInterface(std::shared_ptr<Memory> &process_memory,
                                        ArchEnum expected_arch) {

        if (quicken_interface_) {
            return quicken_interface_.get();
        }

        if (quicken_interface_failed_) {
            return nullptr;
        }

        std::lock_guard<std::mutex> guard(lock_);

        if (!quicken_interface_ & !quicken_interface_failed_) {

            string so_key = name + ":" + to_string(start) + ":" + to_string(end);
            auto it = cached_quicken_interface_.find(so_key);
            if (it != cached_quicken_interface_.end()) {
                quicken_interface_ = it->second;

                elf_load_bias_ = quicken_interface_->GetLoadBias();
                elf_offset = quicken_interface_->GetElfOffset();
                elf_start_offset = quicken_interface_->GetElfStartOffset();

                return quicken_interface_.get();
            }

            Memory *memory = CreateQuickenMemory(process_memory);

            if (memory == nullptr) {
                quicken_interface_failed_ = true;
                QUT_LOG("Create quicken interface failed by creating memory failed.");
                return nullptr;
            }

            unique_ptr<Elf> elf = CreateElf(this->name, memory, expected_arch);

            if (!elf) {
                quicken_interface_failed_ = true;
                QUT_LOG("Create quicken interface failed by creating elf failed.");
                return nullptr;
            }

            string soname = elf->GetSoname();
            string build_id_hex = elf->GetBuildID();
            elf_load_bias_ = elf->GetLoadBias();

            QUT_LOG("GetQuickenInterface elf_offset %llu, offset %llu, elf_load_bias_ %llu"
                    ", soname %s, build_id %s.",
                    (ullint_t) elf_offset, (ullint_t) offset, (ullint_t) elf_load_bias_,
                    soname.c_str(), ToBuildId(build_id_hex).c_str());

            quicken_interface_.reset(CreateQuickenInterfaceFromElf(
                    expected_arch,
                    this->name,
                    soname,
                    elf_load_bias_,
                    elf_offset,
                    elf_start_offset,
                    build_id_hex
            ));

            cached_quicken_interface_[so_key] = quicken_interface_;
        }

        return quicken_interface_.get();
    }

    unique_ptr<Elf> QuickenMapInfo::CreateElf(const string &so_path, Memory *memory,
                                              ArchEnum expected_arch) {

        (void) so_path;

        auto elf = make_unique<Elf>(memory);
        elf->Init();
        if (!elf->valid()) {
            QUT_LOG("elf->valid() so %s invalid", so_path.c_str());
            return nullptr;
        }

        if (elf->arch() != expected_arch) {
            QUT_LOG("elf->arch() invalid %s", so_path.c_str());
            return nullptr;
        }

        return elf;
    }

    QuickenInterface *QuickenMapInfo::CreateQuickenInterfaceFromElf(
            ArchEnum expected_arch,
            const string &so_path,
            const string &so_name,
            const uint64_t load_bias_,
            const uint64_t elf_offset,
            const uint64_t elf_start_offset,
            const string &build_id_hex
    ) {
        std::unique_ptr<QuickenInterface> quicken_interface_ =
                make_unique<QuickenInterface>(load_bias_, elf_offset, elf_start_offset, expected_arch);
        quicken_interface_->SetSoInfo(so_path, so_name, build_id_hex);

        return quicken_interface_.release();
    }

    QuickenInterface *
    QuickenMapInfo::CreateQuickenInterfaceForGenerate(const string &sopath, Elf *elf) {

        string soname = elf->GetSoname();
        string build_id_hex = elf->GetBuildID();
        QuickenInterface *quicken_interface_ = CreateQuickenInterfaceFromElf(
                CURRENT_ARCH,
                sopath,
                soname,
                elf->GetLoadBias(),
                0, // Not use while generating
                0, // Not use while generating
                build_id_hex
        );

        ArchEnum expected_arch = elf->arch();

        ElfInterface *elf_interface = elf->interface();

        if (expected_arch == ARCH_ARM) {
            ElfInterfaceArm *elf_interface_arm = dynamic_cast<ElfInterfaceArm *>(elf_interface);
            quicken_interface_->SetArmExidxInfo(
                    elf_interface_arm->start_offset(),
                    elf_interface_arm->total_entries());
        }

        quicken_interface_->SetEhFrameInfo(
                elf_interface->eh_frame_offset(),
                elf_interface->eh_frame_section_bias(),
                elf_interface->eh_frame_size());
        quicken_interface_->SetEhFrameHdrInfo(
                elf_interface->eh_frame_hdr_offset(),
                elf_interface->eh_frame_hdr_section_bias(),
                elf_interface->eh_frame_hdr_size());
        quicken_interface_->SetDebugFrameInfo(
                elf_interface->debug_frame_offset(),
                elf_interface->debug_frame_section_bias(),
                elf_interface->debug_frame_size());

        ElfInterface *gnu_debugdata_interface = elf_interface->gnu_debugdata_interface();

        if (gnu_debugdata_interface) {
            quicken_interface_->SetGnuEhFrameInfo(
                    gnu_debugdata_interface->eh_frame_offset(),
                    gnu_debugdata_interface->eh_frame_section_bias(),
                    gnu_debugdata_interface->eh_frame_size());
            quicken_interface_->SetGnuEhFrameHdrInfo(
                    gnu_debugdata_interface->eh_frame_hdr_offset(),
                    gnu_debugdata_interface->eh_frame_hdr_section_bias(),
                    gnu_debugdata_interface->eh_frame_hdr_size());
            quicken_interface_->SetGnuDebugFrameInfo(
                    gnu_debugdata_interface->debug_frame_offset(),
                    gnu_debugdata_interface->debug_frame_section_bias(),
                    gnu_debugdata_interface->debug_frame_size());
        }

        return quicken_interface_;
    }

    uint64_t QuickenMapInfo::GetRelPc(uint64_t pc) {
        return pc - start + elf_load_bias_ + elf_offset;
    }

    Memory *QuickenMapInfo::GetFileQuickenMemory() {
        std::unique_ptr<QuickenMemoryFile> memory(new QuickenMemoryFile);
        if (offset == 0) {
            if (memory->Init(name, 0)) {
                return memory.release();
            }
            return nullptr;
        }

        // These are the possibilities when the offset is non-zero.
        // - There is an elf file embedded in a file, and the offset is the
        //   the start of the elf in the file.
        // - There is an elf file embedded in a file, and the offset is the
        //   the start of the executable part of the file. The actual start
        //   of the elf is in the read-only segment preceeding this map.
        // - The whole file is an elf file, and the offset needs to be saved.
        //
        // Map in just the part of the file for the map. If this is not
        // a valid elf, then reinit as if the whole file is an elf file.
        // If the offset is a valid elf, then determine the size of the map
        // and reinit to that size. This is needed because the dynamic linker
        // only maps in a portion of the original elf, and never the symbol
        // file data.
        uint64_t map_size = end - start;
        if (!memory->Init(name, offset, map_size)) {
            return nullptr;
        }

        // Check if the start of this map is an embedded elf.
        uint64_t max_size = 0;
        if (Elf::GetInfo(memory.get(), &max_size)) {
            elf_start_offset = offset;
            if (max_size > map_size) {
                if (memory->Init(name, offset, max_size)) {
                    return memory.release();
                }
                // Try to reinit using the default map_size.
                if (memory->Init(name, offset, map_size)) {
                    return memory.release();
                }
                elf_start_offset = 0;
                return nullptr;
            }
            return memory.release();
        }

        // No elf at offset, try to init as if the whole file is an elf.
        if (memory->Init(name, 0) && Elf::IsValidElf(memory.get())) {
            elf_offset = offset;
            // Need to check how to set the elf start offset. If this map is not
            // the r-x map of a r-- map, then use the real offset value. Otherwise,
            // use 0.
            if (prev_real_map == nullptr || prev_real_map->offset != 0 ||
                prev_real_map->flags != PROT_READ || prev_real_map->name != name) {
                elf_start_offset = offset;
            }
            return memory.release();
        }

        // See if the map previous to this one contains a read-only map
        // that represents the real start of the elf data.
        if (InitFileMemoryFromPreviousReadOnlyMap(memory.get())) {
            return memory.release();
        }

        // Failed to find elf at start of file or at read-only map, return
        // file object from the current map.
        if (memory->Init(name, offset, map_size)) {
            return memory.release();
        }
        return nullptr;
    }

    bool QuickenMapInfo::InitFileMemoryFromPreviousReadOnlyMap(QuickenMemoryFile *memory) {
        // One last attempt, see if the previous map is read-only with the
        // same name and stretches across this map.
        if (prev_real_map == nullptr || prev_real_map->flags != PROT_READ) {
            return false;
        }

        uint64_t map_size = end - prev_real_map->end;
        if (!memory->Init(name, prev_real_map->offset, map_size)) {
            return false;
        }

        uint64_t max_size;
        if (!Elf::GetInfo(memory, &max_size) || max_size < map_size) {
            return false;
        }

        if (!memory->Init(name, prev_real_map->offset, max_size)) {
            return false;
        }

        elf_offset = offset - prev_real_map->offset;
        elf_start_offset = prev_real_map->offset;
        return true;
    }

    Memory *QuickenMapInfo::CreateQuickenMemory(const std::shared_ptr<Memory> &process_memory) {

        (void) process_memory;

        if (end <= start) {
            return nullptr;
        }

        elf_offset = 0;

        // Fail on device maps.
        if (flags & MAPS_FLAGS_DEVICE_MAP) {
            return nullptr;
        }

        // First try and use the file associated with the info.
        if (!name.empty()) {
            Memory *memory = GetFileQuickenMemory();
            if (memory != nullptr) {
                return memory;
            }
        }

        return nullptr;

        // XXX Currently not support memory backed elf.
//        if (process_memory == nullptr) {
//            return nullptr;
//        }
//
//        if (!(flags & PROT_READ)) {
//            return nullptr;
//        }
//
//        // Need to verify that this elf is valid. It's possible that
//        // only part of the elf file to be mapped into memory is in the executable
//        // map. In this case, there will be another read-only map that includes the
//        // first part of the elf file. This is done if the linker rosegment
//        // option is used.
//        std::unique_ptr<MemoryRange> memory(new MemoryRange(process_memory, start, end - start, 0));
//        if (Elf::IsValidElf(memory.get())) {
//            memory_backed_elf = true;
//            return memory.release();
//        }
//
//        // Find the read-only map by looking at the previous map. The linker
//        // doesn't guarantee that this invariant will always be true. However,
//        // if that changes, there is likely something else that will change and
//        // break something.
//        if (offset == 0 || name.empty() || prev_real_map == nullptr ||
//            prev_real_map->name != name ||
//            prev_real_map->offset >= offset) {
//            return nullptr;
//        }
//
//        // Make sure that relative pc values are corrected properly.
//        elf_offset = offset - prev_real_map->offset;
//        // Use this as the elf start offset, otherwise, you always get offsets into
//        // the r-x section, which is not quite the right information.
//        elf_start_offset = prev_real_map->offset;
//
//        MemoryRanges *ranges = new MemoryRanges;
//        ranges->Insert(new MemoryRange(process_memory, prev_real_map->start,
//                                       prev_real_map->end - prev_real_map->start, 0));
//        ranges->Insert(new MemoryRange(process_memory, start, end - start, elf_offset));
//
//        memory_backed_elf = true;
//        return ranges;
    }

    Memory *
    QuickenMapInfo::CreateQuickenMemoryFromFile(const string &so_path,
                                                const uint64_t elf_start_offset) {

        auto memory = make_unique<QuickenMemoryFile>();
        if (memory->Init(string(so_path), elf_start_offset)) {
            return memory.release();
        }

        return nullptr;
    }

    // ---------------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------------

    size_t Maps::GetSize() {
        return maps_size_;
    }

    MapInfoPtr Maps::Find(uint64_t pc) {

        if (!local_maps_) {
            return nullptr;
        }
        size_t first = 0;
        size_t last = maps_size_;

        while (first < last) {
            size_t index = (first + last) / 2;
            const auto &cur = local_maps_[index];
            if (pc >= cur->start && pc < cur->end) {
                return cur;
            } else if (pc < cur->start) {
                last = index;
            } else {
                first = index + 1;
            }
        }
        return nullptr;
    }

    std::vector<MapInfoPtr> Maps::FindMapInfoByName(std::string soname) {

        std::vector<MapInfoPtr> found_mapinfos;
        for (size_t i = 0; i < maps_size_; i++) {
            if (HasSuffix(local_maps_[i]->name, soname)) {
                found_mapinfos.push_back(local_maps_[i]);
            }
        }

        return found_mapinfos;
    }

    void Maps::ReleaseLocalMaps() {
        for (size_t i = 0; i < maps_size_; i++) {
            delete local_maps_[i];
        }

        free(local_maps_);
        maps_capacity_ = 0;
        maps_size_ = 0;
    }

    std::shared_ptr<Maps> Maps::current() {
        if (!current_maps_) {
            Parse();
        }
        std::lock_guard<std::mutex> guard(maps_lock_);
        return current_maps_;
    };

    bool Maps::Parse() {

        std::lock_guard<std::mutex> guard(maps_lock_);

        shared_ptr<Maps> maps = make_shared<Maps>(latest_maps_capacity_);

        bool ret = maps->ParseImpl();

        if (ret) {
            latest_maps_capacity_ = maps->maps_capacity_;
            current_maps_ = move(maps);
        }

        return ret;
    }

    bool Maps::ParseImpl() {
        MapInfoPtr prev_map = nullptr;
        MapInfoPtr prev_real_map = nullptr;

        size_t tmp_capacity = maps_capacity_, tmp_idx = 0;
        MapInfoPtr *tmp_maps = new MapInfoPtr[tmp_capacity];

        bool ret = android::procinfo::ReadMapFile(
                "/proc/self/maps",
                [&](uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff, ino_t,
                    const char *name) {

                    // Mark a device map in /dev/ and not in /dev/ashmem/ specially.
                    if (strncmp(name, "/dev/", 5) == 0 && strncmp(name + 5, "ashmem/", 7) != 0) {
                        flags |= MAPS_FLAGS_DEVICE_MAP;
                    }

                    prev_map = new QuickenMapInfo(prev_map, prev_real_map, start, end, pgoff,
                                                  flags, name);
                    tmp_maps[tmp_idx++] = prev_map;
                    if (!prev_map->IsBlank()) {
                        prev_real_map = prev_map;
                    }

                    if (tmp_idx == tmp_capacity) {
                        tmp_capacity = tmp_capacity + CAPACITY_INCREMENT;
                        MapInfoPtr *swap = new MapInfoPtr[tmp_capacity]();
                        memcpy(swap, tmp_maps, tmp_idx * sizeof(MapInfoPtr));
                        delete[] tmp_maps;    // Only delete array
                        tmp_maps = swap;
                    }

                });

        if (ret) {
            local_maps_ = tmp_maps;
            maps_capacity_ = tmp_capacity;
            maps_size_ = tmp_idx;
        } else {
            // Delete everything
            for (size_t i = 0; i < tmp_idx; i++) {
                delete tmp_maps[i];
            }
            delete[] tmp_maps;
        }

        return ret;
    }

}  // namespace wechat_backtrace
