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
// DynamicLoader.h

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_DYNAMICLOADER_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_DYNAMICLOADER_H_

#include <elf.h>
#include <link.h>

#include <string>

namespace MatrixTracer {
class DynamicLoader {
 private:
    constexpr static uint32_t elfHash(const char *name) {
        if (!name) return 0;
        uint32_t h = 0, g = 0;
        auto pn = reinterpret_cast<const uint8_t *>(name);

        while (*pn) {
            h = (h << 4) + *pn++;
            g = h & 0xf0000000;
            if (g)
                h ^= g >> 24;
            h &= ~g;
        }
        return h;
    }

    constexpr static uint32_t gnuHash(const char *name) {
        if (!name) return 0;
        uint32_t h = 5381;
        auto pn = reinterpret_cast<const uint8_t *>(name);

        for (; *pn; pn++) {
            h = (h << 5) + h + *pn;
        }

        return h;
    }

    static constexpr bool stringStartsWith(std::string_view s, std::string_view prefix) {
        return s.substr(0, prefix.size()) == prefix;
    }

    static constexpr bool stringStartsWith(std::string_view s, char prefix) {
        return !s.empty() && s.front() == prefix;
    }

    static constexpr bool stringEndsWith(std::string_view s, std::string_view suffix) {
        return s.size() >= suffix.size() &&
               s.substr(s.size() - suffix.size(), suffix.size()) == suffix;
    }

    static constexpr bool stringEndsWith(std::string_view s, char prefix) {
        return !s.empty() && s.back() == prefix;
    }

 public:
    struct SymbolName {
        const char * const name;
        const uint32_t elfHash;
        const uint32_t gnuHash;
        SymbolName(const char *n) : name(n),
            elfHash(DynamicLoader::elfHash(name)),
            gnuHash(DynamicLoader::gnuHash(name)) {}
    };

    // std::string has no constexpr constructor in C++17, so default
    // constructor cannot be constexpr here.
    DynamicLoader() {}
    explicit DynamicLoader(const char *soName);
    ~DynamicLoader() = default;

    void *findSymbol(SymbolName sn) const;

    const std::string& fullPath() const { return mFullPath; }

    constexpr bool valid() const noexcept { return mMapAddress != nullptr; }
    constexpr operator bool() const noexcept { return valid(); }

 private:
    using ElfSymbol = ElfW(Sym);
    struct ElfHash;
    struct GnuHash;

    const uint8_t *mMapAddress = nullptr;
    uintptr_t mLoadBias = 0;
    const ElfSymbol *mSymbolTable = nullptr;
    unsigned mSymbolSize = 0;
    const char *mStringTable = nullptr;
    size_t mStringLength = 0;
    const ElfHash *mElfHash = nullptr;
    const GnuHash *mGnuHash = nullptr;

    std::string mFullPath;
};

}  // namespace MatrixTracer

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_DYNAMICLOADER_H_