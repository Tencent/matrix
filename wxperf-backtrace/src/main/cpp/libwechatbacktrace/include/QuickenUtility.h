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

    inline static bool EndsWith(std::string_view s, std::string_view suffix) {
        return s.size() >= suffix.size() &&
               s.substr(s.size() - suffix.size(), suffix.size()) == suffix;
    }

    inline static bool IsOatFile(const std::string &soname) {
        return EndsWith(soname, ".oat") ||
               EndsWith(soname, ".odex") ||
               EndsWith(soname, ".vdex") ||
               EndsWith(soname, ".art");
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
