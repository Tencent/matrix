//
// Created by tomystang on 2021/2/2.
//

#include <fcntl.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/stat.h>
#include <util/Paths.h>

using namespace memguard;

bool memguard::paths::Exists(const std::string &path) {
    return syscall(__NR_faccessat, AT_FDCWD, path.c_str(), F_OK) == 0;
}

std::string memguard::paths::GetParent(const std::string& path) {
    auto lastSlashPos = path.find_last_of("/\\");
    if (lastSlashPos != std::string::npos) {
        return path.substr(0, lastSlashPos);
    } else {
        return path;
    }
}

bool memguard::paths::MakeDirs(const std::string& path, int mode) {
    if (Exists(path)) {
        return true;
    }
    if (!MakeDirs(GetParent(path))) {
        return false;
    }
    if (syscall(__NR_mkdirat, AT_FDCWD, path.c_str(), 0000) != 0) {
        return false;
    }
    return syscall(__NR_fchmodat, AT_FDCWD, path.c_str(), mode, 0) == 0;
}