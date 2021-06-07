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

#ifndef _LIBWECHATBACKTRACE_QUICKEN_TABLE_MANAGER_H
#define _LIBWECHATBACKTRACE_QUICKEN_TABLE_MANAGER_H

#include <cstdint>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "unwindstack/Elf.h"
#include "unwindstack/Memory.h"
#include "BacktraceDefine.h"
#include "Errors.h"
#include "QuickenTable.h"
#include "Log.h"
#include "QuickenInterface.h"

namespace wechat_backtrace {

    class QuickenTableManager {

    private:
        QuickenTableManager() = default;

        ~QuickenTableManager() {};

        QuickenTableManager(const QuickenTableManager &);

        QuickenTableManager &operator=(const QuickenTableManager &);

        /* Never release */
        std::unordered_map<std::string, QutSectionsPtr> qut_sections_map_;
        std::unordered_map<std::string, std::pair<uint64_t, std::string>> qut_sections_requesting_;
        std::unordered_map<std::string, std::string> qut_sections_hash_to_build_id_;
        std::unordered_map<std::string, std::shared_ptr<QuickenInterface>> qut_sections_hash_to_interface_;

        std::mutex lock_;

        static std::string &sPackageName;

        static std::string &sSavingPath;

        static bool sHasWarmedUp;

    public:
        static QuickenTableManager &getInstance() {
            DEFINE_STATIC_LOCAL(QuickenTableManager, instance,);
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
            // XXX Check security here
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

        std::unordered_map<std::string, std::pair<uint64_t, std::string>> GetRequestQut();

        static bool
        CheckIfQutFileExistsWithHash(const std::string &soname, const std::string &hash);

        static bool
        CheckIfQutFileExistsWithBuildId(const std::string &soname, const std::string &build_id);

        QutFileError
        TryLoadQutFile(const std::string &soname, const std::string &sopath,
                       const std::string &hash, const std::string &build_id,
                       QutSectionsPtr &qut_sections, const bool testOnly = false);

        QutFileError
        RequestQutSections(const std::string &soname, const std::string &sopath,
                           const std::string &hash, const std::string &build_id,
                           const uint64_t elf_start_offset,
                           QutSectionsPtr &qut_sections);

        QutFileError
        SaveQutSections(const std::string &soname, const std::string &sopath,
                        const std::string &hash, const std::string &build_id,
                        const bool only_save_file,
                        std::unique_ptr<QutSections> qut_sections);

        QutFileError
        FindQutSectionsNoLock(const std::string &soname, const std::string &sopath,
                              const std::string &hash, const std::string &build_id,
                              const uint64_t elf_start_offset,
                              QutSectionsPtr &qut_sections_ptr);

        bool
        InsertQutSectionsNoLock(const std::string &soname, const std::string &hash,
                                const std::string &build_id,
                                QutSectionsPtr &qut_sections,
                                bool immediately);

        void
        EraseQutRequestingByHash(const std::string &hash);

        void
        RecordQutRequestInterface(std::shared_ptr<QuickenInterface> &self_ptr);

    };
}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUICKEN_TABLE_MANAGER_H
