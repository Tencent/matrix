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

#include "avoid_select_all_checker.h"
#include "core/lint_logic.h"
#include "comm/lint_util.h"
#include <comm/log/logger.h>

namespace sqlitelint {

    // the select all judgement is actually done in the sql pre-process step
    void AvoidSelectAllChecker::Check(LintEnv &env, const SqlInfo &sql_info,
                                      std::vector<Issue> *issues) {
        if (env.IsInWhiteList(kCheckerName, sql_info.wildcard_sql_)
                || env.IsInWhiteList(kCheckerName, sql_info.sql_)) {
            sVerbose("AvoidSelectAllChecker::Check in white list: %s; %s", sql_info.wildcard_sql_.c_str(), sql_info.sql_.c_str());
            return;
        }

        if (sql_info.is_select_all_) {
            PublishIssue(env, sql_info, issues);
        }
    }

    CheckScene AvoidSelectAllChecker::GetCheckScene() {
        return CheckScene::kUncheckedSql;
    }

    void AvoidSelectAllChecker::PublishIssue(const LintEnv& env, const SqlInfo& sql_info, std::vector<Issue> *issues) {
        const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;
        sVerbose("AvoidSelectAllChecker::PublishIssue sql:%s", wildcard_sql.c_str());

        Issue issue;
        issue.id = GenIssueId(env.GetDbFileName(), kCheckerName, wildcard_sql);
        issue.db_path = env.GetDbPath();
        issue.create_time = GetSysTimeMillisecond();
        issue.level = IssueLevel::kTips;
        issue.type = IssueType::kAvoidSelectAllChecker;
        issue.sql = sql_info.sql_ ;
        issue.desc = "Found select * sql:" + sql_info.sql_ ;
        issue.advice = "It is recommended only select the required columns.";
        issue.ext_info = sql_info.ext_info_;
        issue.sql_time_cost = sql_info.time_cost_;
        issue.is_in_main_thread = sql_info.is_in_main_thread_;

        issues->push_back(issue);
    }
}
