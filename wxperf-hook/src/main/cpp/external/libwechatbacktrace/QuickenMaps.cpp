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

#include "ElfInterfaceArm.h"
#include "QuickenMaps.h"

namespace wechat_backtrace {

using namespace std;
using namespace unwindstack;

#define CAPACITY_INCREMENT 1024

mutex Maps::maps_lock_;
size_t Maps::latest_maps_capacity_ = CAPACITY_INCREMENT;
shared_ptr<Maps> Maps::current_maps_;

QuickenInterface* QuickenMapInfo::GetQuickenInterface(const shared_ptr<Memory>& process_memory,
        ArchEnum expected_arch) {

    if (quicken_interface_) {
        return quicken_interface_.get();
    }

    std::lock_guard<std::mutex> guard(lock_);

    if (!quicken_interface_) {

        Elf* elf = GetElf(process_memory, expected_arch);

        if (!elf->valid()) {
            return nullptr;
        }

        ElfInterfaceArm* elf_interface = dynamic_cast<ElfInterfaceArm *>(elf->interface());

        quicken_interface_.reset(new QuickenInterface(elf->memory(), elf->GetLoadBias(),
                                                      elf_interface->start_offset(),
                                                      elf_interface->total_entries()));

        elf_load_bias_ = elf->GetLoadBias();
    }

    return quicken_interface_.get();
}

FastArmExidxInterface* QuickenMapInfo::GetFastArmExidxInterface(const shared_ptr<Memory>& process_memory,
        ArchEnum expected_arch) {

    if (exidx_interface_) {
        return exidx_interface_.get();
    }

    std::lock_guard<std::mutex> guard(lock_);

    if (!exidx_interface_) {

        Elf* elf = GetElf(process_memory, expected_arch);

        if (!elf->valid()) {
            return nullptr;
        }

        ElfInterfaceArm* elf_interface = dynamic_cast<ElfInterfaceArm *>(elf->interface());

        exidx_interface_.reset(new FastArmExidxInterface(elf->memory(), elf->GetLoadBias(),
                                                      elf_interface->start_offset(),
                                                      elf_interface->total_entries()));

        elf_load_bias_ = elf->GetLoadBias();
    }

    return exidx_interface_.get();
}

uint64_t QuickenMapInfo::GetRelPc(uint64_t pc) {
    return pc - start + elf_load_bias_ + elf_offset;
}


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

inline bool Maps::HasSuffix(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
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
    for (size_t i = 0; i < maps_capacity_; i++) {
        delete local_maps_[i];
    }

    delete local_maps_;
    maps_capacity_ = 0;
    maps_size_ = 0;
    local_maps_ = nullptr;
}

std::shared_ptr<Maps> Maps::current() {
    std::lock_guard<std::mutex> guard(maps_lock_);
    return current_maps_;
};

bool Maps::Parse() {
    std::lock_guard<std::mutex> guard(maps_lock_);

    shared_ptr<Maps> maps(new Maps(latest_maps_capacity_));

    bool ret = maps->ParseImpl();

    if (ret) {
        current_maps_ = maps;
        latest_maps_capacity_ = maps->maps_capacity_;
    }

    return ret;
}

bool Maps::ParseImpl() {
    MapInfoPtr prev_map = nullptr;
    MapInfoPtr prev_real_map = nullptr;

    size_t tmp_capacity = maps_capacity_, tmp_idx = 0;
    MapInfoPtr *tmp_maps = nullptr;
    tmp_maps = new MapInfoPtr[tmp_capacity]();

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
                    delete tmp_maps;    // Only delete array
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
        delete tmp_maps;
    }

    return ret;
}

}  // namespace wechat_backtrace
