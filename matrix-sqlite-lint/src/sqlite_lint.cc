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
//  Created by liyongjie on 2017/2/23.
//  Copyright © 2017年 tencent. All rights reserved.
//

#include "sqlite_lint.h"
#include "core/lint_manager.h"
#include <thread>

namespace sqlitelint {

    SqlExecutionDelegate kSqlExecutionDelegate;

    void SetSqlExecutionDelegate(SqlExecutionDelegate func){
        kSqlExecutionDelegate = func;
    }

    void InstallSQLiteLint(const char* db_path, OnPublishIssueCallback issue_callback) {
        LintManager::Get()->Install(db_path, issue_callback);
    }

    void UninstallSQLiteLint(const char* db_path) {
        std::thread uninstall_thread(&LintManager::Uninstall, LintManager::Get(), std::string(db_path));
        uninstall_thread.detach();
//        LintManager::Get()->Uninstall(db_path);
    }

    void NotifySqlExecution(const char* db_path, const char* sql, long time_cost, const char* ext_info) {
        LintManager::Get()->NotifySqlExecution(db_path, sql, time_cost, ext_info);
    }

    void SetWhiteList(const char* db_path, const std::map<std::string, std::set<std::string>>& white_list) {
        LintManager::Get()->SetWhiteList(db_path, white_list);
    }

    void EnableChecker(const char* db_path, const std::string& checker_name) {
        LintManager::Get()->EnableChecker(db_path, checker_name);
    }
}
