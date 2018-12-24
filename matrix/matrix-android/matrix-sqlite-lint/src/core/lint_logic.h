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
// Some logic functions
// Created by liyongjie on 2017/6/21
//

#ifndef SQLITE_LINT_LINT_LOGIC_H
#define SQLITE_LINT_LINT_LOGIC_H

#include <string>
#include <vector>
#include "lint_info.h"

namespace sqlitelint {
    // Gen id to indentify a issue
    // identity_info can differ this issue from others in a certain checker(check_name)
    std::string GenIssueId(const std::string& db_file_name, const std::string& checker_name, const std::string& identity_info);

    // Now only "select", "insert", "update", "delete", "create" supported
    bool IsSqlSupportCheck(const std::string& sql);

    constexpr static const char* const  kAutoIndexPrefix = "sqlite_autoindex_";
    static const int kAutoIndexPrefixLen = strlen(kAutoIndexPrefix);
    bool IsSQLite3AutoIndex(const std::string& index);

    void DumpQueryPlans(const std::vector<Record> & plans);
}

#endif //SQLITE_LINT_LINT_LOGIC_H
