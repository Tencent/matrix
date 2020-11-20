#ifndef _LIBWECHATBACKTRACE_QUICKEN_UTILITY_H
#define _LIBWECHATBACKTRACE_QUICKEN_UTILITY_H

#include "../../common/Log.h"
#include "SHA1.hpp"

namespace wechat_backtrace {

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

    inline static std::string ToBuildId(const std::string build_id_raw) {
        const size_t len = build_id_raw.length();
        std::string build_id(len * 2, '\0');

        for (size_t i = 0; i < len; i++) {
            unsigned int n = build_id_raw[i];
            build_id[i * 2] = "0123456789ABCDEF"[(n >> 4) % 16];
            build_id[i * 2 + 1] = "0123456789ABCDEF"[n % 16];
        }

        QUT_LOG("build_id %s %d", build_id.c_str(), build_id.length());

        return build_id;
    }

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_UTILITY_H
