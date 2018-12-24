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
//  The apis exposed to SDK-Level; eg.android, iOS, windows
//
//  Created by liyongjie on 2017/2/23.
//  Copyright © 2017年 tencent. All rights reserved.
//
#ifndef SQLITE_LINT_SQLITE_LINT_H
#define SQLITE_LINT_SQLITE_LINT_H

#include <map>
#include <string>
#include <set>
#include <vector>
#include "slint.h"

namespace sqlitelint {

    typedef int(*SqlExecutionCallback)(void*, int, char**, char**);

    // SQLiteLint will delegate the sql execution to the platform-SDK level.
    // TODO redefine params
    typedef int(*SqlExecutionDelegate)(const char *dbPath, const char *sql, SqlExecutionCallback callback, void *para, char **errmsg);
    void SetSqlExecutionDelegate(SqlExecutionDelegate delegate);

    // It's a listener of the SDK level to be notified the issue published by some checkers
    typedef void(*OnPublishIssueCallback) (const char* db_path, std::vector<Issue> published_issues);

    // SDK level call this function to install a SQLiteLint with the concerned db_path
    void InstallSQLiteLint(const char* db_path, OnPublishIssueCallback issue_callback);
    void UninstallSQLiteLint(const char* db_path);

    // SDK level call this function to notify the sqls executed
    // ext_info will be transmitted transparently back to the SDK level through OnPublishIssueCallback
    void NotifySqlExecution(const char* db_path, const char* sql, long time_cost, const char* ext_info);

    // SDK level call this to set white list
    void SetWhiteList(const char* db_path, const std::map<std::string, std::set<std::string>>& white_list);

    // SDK level call this to enable A checker
    void EnableChecker(const char* db_path, const std::string& checker_name);
}

#endif /* SQLITE_LINT_SQLITE_LINT_H */
