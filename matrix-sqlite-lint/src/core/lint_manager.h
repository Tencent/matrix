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

//
// Created by liyongjie
//

#ifndef SQLITE_LINT_LINT_MANAGER_H
#define SQLITE_LINT_LINT_MANAGER_H

#include <map>
#include <mutex>

#include "sqlite_lint.h"

namespace sqlitelint {

    class Lint;

    // A singleton and it manage the lint
    class LintManager {
    public :
        static LintManager* Get();
        static void Release();
        void Install(const char* db_path, OnPublishIssueCallback issued_callback);
        void Uninstall(const std::string& db_path);
        void UninstallAll();
        void NotifySqlExecution(const char* db_path, const char* sql, long time_cost, const char* ext_info);
        void SetWhiteList(const char* db_path, const std::map<std::string, std::set<std::string>>& white_list);
        void EnableChecker(const char* db_path, const std::string& checker_name);

    private:
        LintManager(){};
        ~LintManager(){};
    private:
        std::map<const std::string, Lint*> lints_;
        static std::mutex lints_mutex_;
        static LintManager *instance_;
    };
}
#endif //SQLITE_LINT_LINT_MANAGER_H
