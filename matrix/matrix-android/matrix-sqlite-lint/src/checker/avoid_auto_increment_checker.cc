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

#include "avoid_auto_increment_checker.h"
#include "core/lint_logic.h"
#include "comm/lint_util.h"
#include <comm/log/logger.h>

namespace sqlitelint {

    void AvoidAutoIncrementChecker::Check(LintEnv &env, const SqlInfo &sql_info,
                                          std::vector<Issue> *issues) {
        std::vector<TableInfo> tables = env.GetTablesInfo();

        sVerbose("AvoidAutoIncrementChecker::Check tables count: %zu", tables.size());

        std::string create_sql;

        for (const TableInfo& table_info : tables) {
            if (env.IsInWhiteList(kCheckerName, table_info.table_name_)) {
                sVerbose("AvoidAutoIncrementChecker::Check in white list: %s", table_info.table_name_.c_str());
                continue;
            }

            create_sql = table_info.create_sql_;
            ToLowerCase(create_sql);
            if (create_sql.find(kAutoIncrementKeyWord) != std::string::npos) {
                PublishIssue(env, table_info.table_name_, issues);
            }
        }
    }

    CheckScene AvoidAutoIncrementChecker::GetCheckScene() {
        return CheckScene::kAfterInit;
    }

    void AvoidAutoIncrementChecker::PublishIssue(const LintEnv& env, const std::string &table_name,
                                                 std::vector<Issue> *issues) {
        sVerbose("AvoidAutoIncrementChecker::PublishIssue table: %s", table_name.c_str());

        std::string desc = "Table(" + table_name + ") has a column which is AutoIncrement."
                            + "It's not really recommended.";

        Issue issue;
        issue.id = GenIssueId(env.GetDbFileName(), kCheckerName, table_name);
        issue.db_path = env.GetDbPath();
        issue.create_time = GetSysTimeMillisecond();
        issue.level = IssueLevel::kTips;
        issue.type = IssueType::kAvoidAutoIncrement;
        issue.table = table_name;
        issue.desc = desc;

        issues->push_back(issue);
    }
}
