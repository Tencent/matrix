//
// Created by tomystang on 2021/2/2.
//

#ifndef __MEMGUARD_PATHS_H__
#define __MEMGUARD_PATHS_H__


#include <string>
#include <linux/stat.h>

namespace memguard {
    namespace paths {
        constexpr const int kDefaultDirectoryPermission = 00775;
        constexpr const int kDefaultDataFilePermission = 00664;

        extern bool Exists(const std::string& path);
        extern std::string GetParent(const std::string& path);
        extern bool MakeDirs(const std::string& path, int mode = kDefaultDirectoryPermission);
    }
}


#endif //__MEMGUARD_PATHS_H__
