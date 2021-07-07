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

// Author: leafjia@tencent.com
//
// DynamicLoader.cpp

#include "DynamicLoader.h"

#include <sys/mman.h>
#include <elf.h>

#include <type_traits>
#include <limits>

#include "SafeMaps.h"


#define uint8_tp uint8_t*
#define Tp T*

namespace MatrixTracer {

using ElfHeader = ElfW(Ehdr);
using ProgramHeader = ElfW(Phdr);
using DynamicTag = ElfW(Dyn);
using SectionHeader = ElfW(Shdr);
using ElfOffset = ElfW(Off);

struct DynamicLoader::ElfHash {
    uint32_t bucketCount;
    uint32_t chainCount;
    uint32_t buckets[0];
};

struct DynamicLoader::GnuHash {
    uint32_t bucketCount;
    uint32_t symbolOffset;
    uint32_t bloomSize;
    uint32_t bloomShift;
    ElfOffset bloom[0];
};

class MemoryRegion {
    const uint8_t * const start;
    const size_t size;

 public:
    constexpr MemoryRegion(void *s, void *e) :
            start(static_cast<uint8_t *>(s)),
            size(static_cast<uint8_t *>(e) > s ? static_cast<uint8_t *>(e) - start : 0) {}

    constexpr bool valid(size_t off, size_t s) { return off + s < size; }

    template <typename T, std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
    constexpr T get(size_t off) { return *reinterpret_cast<const T*>(start + off); }

    template <typename T, std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
    constexpr const T* cast(size_t off) { return reinterpret_cast<const T*>(start + off); }
};

DynamicLoader::DynamicLoader(const char *soName) {
    std::string_view name(soName);
    bool isAbsolute = stringStartsWith(name, '/');
    void *mapStart = nullptr, *mapEnd = nullptr;

    // Scan maps for the dynamic library image.
    readSelfMaps([&, isAbsolute](uintptr_t start, uintptr_t end, uint16_t flags,
                                 uint64_t offset, ino_t, std::string_view path) -> bool {
        if (!mapStart) {
            // We want region whose:
            //  1. offset is 0
            //  2. flags is READ|EXEC
            //  3. path matches the soName
            if (offset != 0) return false;
            if ((flags & PROT_READ) == 0) return false;
            if (isAbsolute) {
                if (path != name) return false;
            } else {
                if (!stringEndsWith(path, name)) return false;
                if (!stringEndsWith(path.substr(0, path.size() - name.size()), '/'))
                    return false;
            }
            mapStart = reinterpret_cast<void *>(start);
            mapEnd = reinterpret_cast<void *>(end);
            mFullPath = path;
        } else if ((path == mFullPath || (path.empty() && flags == 0)) &&
                   reinterpret_cast<void *>(start) == mapEnd) {
            // Found continuous region, extend the mapEnd region.
            mapEnd = reinterpret_cast<void *>(end);
        } else {
            // No more continuous region, end searching.
            return true;
        }
        return false;
    });
    if (!mapStart || !mapEnd) return;

    // Parse ELF header.
    MemoryRegion mem(mapStart, mapEnd);
    if (!mem.valid(0, sizeof(ElfHeader))) return;
    auto elfHeader = mem.cast<ElfHeader>(0);

    // Parse Program header for load bias and dynamic section.
    if (!mem.valid(elfHeader->e_phoff, elfHeader->e_phentsize * elfHeader->e_phnum))
        return;
    auto programHead = mem.cast<ProgramHeader>(elfHeader->e_phoff);
    uintptr_t dynamicVaddr = 0;
    for (int i = 0; i < elfHeader->e_phnum; i++) {
        auto ph = programHead + i;
        if (ph->p_type == PT_LOAD && (ph->p_flags & PF_X) && ph->p_offset == 0) {
            mLoadBias = ph->p_vaddr;
        } else if (ph->p_type == PT_DYNAMIC) {
            dynamicVaddr = ph->p_vaddr;
        }
    }
    if (dynamicVaddr == 0) return;
    dynamicVaddr -= mLoadBias;

    // Parse dynamic section for .dynsym / .dynstr / .hash sections.
    for (unsigned i = 0; mem.valid(dynamicVaddr + i * sizeof(DynamicTag),
            sizeof(DynamicTag)); i++) {
        auto dyn = mem.cast<DynamicTag>(dynamicVaddr + i * sizeof(DynamicTag));

        switch (dyn->d_tag) {
            case DT_NULL: goto endLoop;
            case DT_HASH: mElfHash = mem.cast<ElfHash>(dyn->d_un.d_ptr - mLoadBias); break;
            case DT_GNU_HASH: mGnuHash = mem.cast<GnuHash>(dyn->d_un.d_ptr - mLoadBias); break;
            case DT_SYMTAB: mSymbolTable = mem.cast<ElfSymbol>(dyn->d_un.d_ptr - mLoadBias); break;
            case DT_SYMENT: mSymbolSize = dyn->d_un.d_val; break;
            case DT_STRTAB: mStringTable = mem.cast<char>(dyn->d_un.d_ptr - mLoadBias); break;
            case DT_STRSZ: mStringLength = dyn->d_un.d_val; break;
        }
    }
    endLoop:
    if (!mSymbolTable || !mStringTable || !mSymbolSize || !mStringLength) return;
    if (!mElfHash && !mGnuHash) return;

    mMapAddress = static_cast<uint8_t *>(mapStart);
    assert(!mFullPath.empty());
}

template <typename T>
static constexpr T* byteOffset(T* ptr, size_t off) {
    return (Tp)((uint8_tp) ptr + off);
}

void *DynamicLoader::findSymbol(DynamicLoader::SymbolName sn) const {
    if (!valid()) return nullptr;

    if (mGnuHash) {
        // GNU lookup.
        constexpr size_t elfBits = sizeof(ElfOffset) * 8;
        uint32_t bucketCount = mGnuHash->bucketCount;
        uint32_t symOffset = mGnuHash->symbolOffset;
        uint32_t bloomSize = mGnuHash->bloomSize;
        uint32_t bloomShift = mGnuHash->bloomShift;
        const ElfOffset *bloom = mGnuHash->bloom;
        const uint32_t *buckets = reinterpret_cast<const uint32_t *>(&bloom[bloomSize]);
        const uint32_t *chain = &buckets[bucketCount];

        ElfOffset word = bloom[(sn.gnuHash / elfBits) % bloomSize];
        ElfOffset mask = static_cast<ElfOffset>(1) << (sn.gnuHash % elfBits)
                         | static_cast<ElfOffset>(1) << ((sn.gnuHash >> bloomShift) % elfBits);

        if ((word & mask) != mask)
            return nullptr;

        uint32_t n = buckets[sn.gnuHash % bucketCount];
        if (n < symOffset)
            return nullptr;

        while (true) {
            const ElfSymbol *sym = byteOffset(mSymbolTable, mSymbolSize * n);

            if (sym->st_name >= mStringLength) return nullptr;
            const char *symName = &mStringTable[sym->st_name];
            const uint32_t hash = chain[n - symOffset];

            if ((sn.gnuHash | 1) == (hash | 1) &&
                !strncmp(symName, sn.name, mStringLength - sym->st_name)) {
                return const_cast<uint8_t *>(mMapAddress) + sym->st_value - mLoadBias;
            }

            if (hash & 1) break;
            n++;
        }
        return nullptr;
    } else if (mElfHash) {
        // ELF lookup.
        uint32_t bucketCount = mElfHash->bucketCount;
        uint32_t chainCount = mElfHash->chainCount;
        const uint32_t *bucket = mElfHash->buckets;
        const uint32_t *chain = &bucket[bucketCount];
        for (uint32_t n = bucket[sn.elfHash % bucketCount]; n != 0; n = chain[n]) {
            if (n >= chainCount) break;
            const ElfSymbol *sym = byteOffset(mSymbolTable, mSymbolSize * n);

            // Get symbol name from string table.
            if (sym->st_name >= mStringLength) return nullptr;
            const char *symName = &mStringTable[sym->st_name];

            if (!strncmp(symName, sn.name, mStringLength - sym->st_name))
                return const_cast<uint8_t *>(mMapAddress) + sym->st_value - mLoadBias;
            else
                return nullptr;
        }
        return nullptr;
    } else {
        return nullptr;
    }
}
}   // namespace MatrixTracer
