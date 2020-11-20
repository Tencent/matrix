//
// Created by Carl on 2020-09-21.
//

#ifndef _LIBWECHATBACKTRACE_QUICKEN_TABLE_MANAGER_H
#define _LIBWECHATBACKTRACE_QUICKEN_TABLE_MANAGER_H

#include <cstdint>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "unwindstack/Elf.h"
#include "unwindstack/Memory.h"
#include "Predefined.h"
#include "Errors.h"
#include "QuickenTable.h"
#include "../../common/Log.h"

namespace wechat_backtrace {

    enum QutFileError : uint16_t {
        NoneError = 0,
        NotInitialized,
        NotWarmedUp,
        LoadRequesting,
        OpenFileFailed,
        FileStateError,
        FileTooShort,
        MmapFailed,
        QutVersionNotMatch,
        ArchNotMatch,
        BuildIdNotMatch,
        FileLengthNotMatch,
        InsertNewQutFailed,
    };

    class QuickenTableManager {

    private:
        QuickenTableManager() = default;

        ~QuickenTableManager() {};

        QuickenTableManager(const QuickenTableManager &);

        QuickenTableManager &operator=(const QuickenTableManager &);

        std::unordered_map<std::string, QutSections *> qut_sections_map_;          // TODO destruction
        std::unordered_map<std::string, std::string> qut_sections_requesting_;   // TODO destruction

        std::mutex lock_;

        static std::string sPackageName;

        static std::string sSavingPath;

        static bool sHasWarmedUp;

    public:
        static QuickenTableManager &getInstance() {
            static QuickenTableManager instance;
            return instance;
        }

        static bool MakeDir(const char *const path) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
            struct stat st = {0};
            if (stat(path, &st) == -1) {
                return mkdir(path, 0700) == 0;
            }
#pragma clang diagnostic pop
            return false;
        }

        static void SetPackageName(const std::string &package_name) {
            sPackageName = package_name;
        }

        static void SetSavingPath(const std::string &saving_path) {
            // TODO Check security here
            std::string arch_folder = CURRENT_ARCH_ENUM == QUT_ARCH_ARM ? "arm32" : "arm64";
            sSavingPath = saving_path + arch_folder;
            MakeDir(sSavingPath.c_str());
        }

        static void WarmUp(bool warm_up) {
            QUT_LOG("Warm up set %d", warm_up);
            sHasWarmedUp = warm_up;
        }

        static bool HasWarmedUp() {
            return sHasWarmedUp;
        }

        std::unordered_map<std::string, std::string> GetRequestQut();

        static bool
        CheckIfQutFileExistsWithHash(const std::string &soname, const std::string &hash);

        static bool
        CheckIfQutFileExistsWithBuildId(const std::string &soname, const std::string &build_id);

        QutFileError
        RequestQutSections(const std::string &soname, const std::string &sopath,
                           const std::string &hash, const std::string &build_id,
                           const std::string &build_id_hex, QutSectionsPtr &qut_sections);

        QutFileError
        SaveQutSections(const std::string &soname, const std::string &sopath,
                        const std::string &hash, const std::string &build_id,
                        const std::string &build_id_hex, QutSectionsPtr qut_sections);

        QutFileError
        FindQutSections(const std::string &soname, const std::string &sopath,
                        const std::string &build_id,
                        QutSectionsPtr &qut_sections_ptr);

        bool
        InsertQutSections(const std::string &build_id, QutSectionsPtr qut_sections, bool immediately);
    };
}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_TABLE_MANAGER_H
