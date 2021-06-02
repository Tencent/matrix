/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef _LIBWECHATBACKTRACE_QUICKEN_UTILITY_H
#define _LIBWECHATBACKTRACE_QUICKEN_UTILITY_H

#include <fcntl.h>
#include "Log.h"
#include "SHA1.h"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    inline static bool HasSuffix(const std::string &str, const std::string &suffix) {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    inline static std::string SplitSonameFromPath(const std::string &sopath) {
        size_t pos = sopath.find_last_of(FILE_SEPERATOR);
        return sopath.substr(pos + 1);
    }

    inline static bool EndsWith(std::string const &value, std::string const &ending) {
        if (ending.size() > value.size()) return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    inline static std::string RemoveMapsDeleteSuffix(const std::string &maps_name) {
        if (EndsWith(maps_name, " (deleted)")) {
            return maps_name.substr(0, maps_name.length() - 10);
        }
        return maps_name;
    }

    inline static std::string ToHash(const std::string &sopath) {
        SHA1 sha1;
        sha1.update(sopath);
        std::string hash = sha1.final();
        return hash;
    }

    static const char *HexChars = "0123456789ABCDEF";

    inline static std::string ToBuildId(const std::string &build_id_raw) {
        const size_t len = build_id_raw.length();
        std::string build_id(len * 2, '\0');

        for (size_t i = 0; i < len; i++) {
            unsigned int n = build_id_raw[i];
            build_id[i * 2] = HexChars[(n >> 4) % 16];
            build_id[i * 2 + 1] = HexChars[n % 16];
        }

        return build_id;
    }

    inline bool StartsWith(std::string_view s, std::string_view prefix) {
        return s.substr(0, prefix.size()) == prefix;
    }

    inline static bool EndsWith(std::string_view s, std::string_view suffix) {
        return s.size() >= suffix.size() &&
               s.substr(s.size() - suffix.size(), suffix.size()) == suffix;
    }

    inline static bool IsSoFile(const std::string &soname) {
        return EndsWith(soname, ".so");
    }

    inline static bool IsOatFile(const std::string &soname) {
        return EndsWith(soname, ".oat") ||
               EndsWith(soname, ".odex");
    }

    inline static bool IsJitCacheMap(const std::string &name) {
        return StartsWith(name, "/memfd:jit-cache") ||
               StartsWith(name, "/memfd:/jit-cache") ||
               EndsWith(name, "jit-code-cache]");
    }

    inline static bool MaybeDexFile(const std::string &soname) {
        return EndsWith(soname, ".jar") ||
               EndsWith(soname, ".apk") ||
               EndsWith(soname, ".vdex") ||
               EndsWith(soname, ".dex");
    }

    inline static size_t FileSize(const std::string &sopath) {
        int fd = open(sopath.c_str(), O_RDONLY);
        size_t file_size = 0;
        if (fd >= 0) {
            struct stat file_stat;
            if (fstat(fd, &file_stat) == 0) {
                file_size = file_stat.st_size;
            }
            close(fd);
        }

        return file_size;
    }

    inline static std::string FakeBuildId(const std::string &sopath) {
        std::string build_id = "";
        if (IsOatFile(sopath)) {
            int fd = open(sopath.c_str(), O_RDONLY);
            if (fd >= 0) {
                struct stat file_stat;
                if (fstat(fd, &file_stat) == 0 && file_stat.st_size > 0) {
                    std::string build_id_raw =
                            std::to_string((ullint_t) file_stat.st_size) + sopath +
                            std::to_string((ullint_t) file_stat.st_mtim.tv_sec);
                    build_id = ToHash(build_id_raw);
                }
                close(fd);
            }
        }

        return build_id;
    }

    QUT_EXTERN_C_BLOCK_END

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UTILITY_H
