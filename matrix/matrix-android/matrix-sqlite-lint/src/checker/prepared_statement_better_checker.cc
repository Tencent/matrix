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
// Author: liyongjie
// Created by liyongjie
//

#include "prepared_statement_better_checker.h"
#include <core/lint_logic.h>
#include <comm/lint_util.h>
#include <comm/log/logger.h>

namespace sqlitelint {

    void PreparedStatementBetterChecker::Check(LintEnv &env, const SqlInfo &sql_info,
                                               std::vector <Issue> *issues) {

        sVerbose("PreparedStatementBetterChecker::Check");

        std::vector<SqlInfo>* to_check_sqls = new std::vector<SqlInfo>;
        std::map<std::string, std::vector<SqlInfo*>>* buckets = new std::map<std::string, std::vector<SqlInfo*>>;

        HandleTheBoundary(env, to_check_sqls);

        MakeGroup(*to_check_sqls, buckets);

        std::vector<SqlInfo*>* section = new std::vector<SqlInfo*>;

        for (auto it = buckets->begin(); it != buckets->end();it++) {
            const std::vector<SqlInfo*>& group_sqls = it->second;
            if (group_sqls.empty()) {
                continue;
            }

            if (env.IsInWhiteList(kCheckerName, group_sqls[0]->wildcard_sql_)
                    || env.IsInWhiteList(kCheckerName, group_sqls[0]->sql_)) {
                sVerbose("PreparedStatementBetterChecker::Check in white list: %s", group_sqls[0]->wildcard_sql_.c_str());
                continue;
            }

            section->clear();

            ScanCheck(group_sqls, section);

            if (!section->empty()) {
                PublishIssue(env, *section, issues);
            }
        }

        delete section;
        delete to_check_sqls;
        delete buckets;
    }

    //TODO env const
    void PreparedStatementBetterChecker::HandleTheBoundary(LintEnv &env,
                                                           std::vector<SqlInfo>* to_check_sqls) {
        std::vector<SqlInfo> sql_history = env.GetSqlHistory();
        const int st = sql_history.size() - GetSqlCntToSample();
        // include the last #GetSqlCntToSample() sqls
        for (int i = st > 0 ? st : 0; i<sql_history.size(); i++) {
            to_check_sqls->push_back(sql_history[i]);
        }
        //look backward to include a little more sqls
        if (st > 0) {
            const SqlInfo& reference = sql_history.at(st);
            const int threshold = kContinuityInternalThreshold * (kContinuousCountThreshold - 1);
            for (int j = st - 1; j >= 0 && j < sql_history.size(); j--) {
                const SqlInfo& target = sql_history.at(j);
                if (reference.execution_time_ - target.execution_time_ >= threshold) {
                    break;
                }

                to_check_sqls->insert(to_check_sqls->begin(), target);
            }
        }
    }

    void PreparedStatementBetterChecker::MakeGroup(const std::vector<SqlInfo>& to_check_sqls,
                                                   std::map<std::string, std::vector<SqlInfo*>>* buckets) {
        for (const SqlInfo& info : to_check_sqls) {
            if (info.is_prepared_statement_) {
                continue;
            }
            const std::string& statement_sql = !info.wildcard_sql_.empty() ? info.wildcard_sql_ : info.sql_;

            if (statement_sql.empty()) {
                sInfo("PreparedStatementBetterChecker::MakeGroup statement_sql still empty");
                continue;
            }

            if (buckets->find(statement_sql) == buckets->end()) {
                buckets->insert(std::pair<std::string,std::vector<SqlInfo*>> (statement_sql, std::vector<SqlInfo*>()));
            }
            (*buckets)[statement_sql].push_back(const_cast<SqlInfo*> (&info));
        }
    }

    void PreparedStatementBetterChecker::ScanCheck(const std::vector<SqlInfo*> &grouped_sqls,
                                                   std::vector<SqlInfo*>* section) {
        int st=0, end=1;
        int size = grouped_sqls.size();
        while (st < size && end < size) {
            int end_adjacent_interval = grouped_sqls[end]->execution_time_ - grouped_sqls[end - 1]->execution_time_;
            if (end_adjacent_interval < kContinuityInternalThreshold) {
                end ++;
            } else {
                if (end - st >= kContinuousCountThreshold) {
                    // find one section is enough
                    break;
                }

                st = end;
                end = end + 1;
            }
        }

        if (end - st >= kContinuousCountThreshold) {
            for (int i = st; i < end; i++) {
                section->push_back(grouped_sqls[i]);
            }
        }
    }

    void PreparedStatementBetterChecker::PublishIssue(const LintEnv& env, const std::vector<SqlInfo*>& section,
                                                      std::vector<Issue> *issues) {
        std::string desc = "The following sql executed continuously:\n";
        std::string identity_info = "";

        sVerbose("PreparedStatementBetterChecker::PublishIssue %s,size %d", section[0]->wildcard_sql_.c_str(),section.size());
        for (SqlInfo* sql_info : section) {
            desc += FormatTime(sql_info->execution_time_/1000);
            desc += ":\n";
            desc += sql_info->sql_;
            desc += "\n\n";

            if (identity_info == "") {
                identity_info = sql_info->wildcard_sql_.empty() ? sql_info->sql_ : sql_info->wildcard_sql_;
            }
        }

        if (identity_info == "") {
            identity_info = desc;
        }

        Issue issue;
        issue.id = GenIssueId(env.GetDbFileName(), kCheckerName, identity_info);
        issue.db_path = env.GetDbPath();
        issue.create_time = GetSysTimeMillisecond();
        issue.level = IssueLevel::kSuggestion;
        issue.type = IssueType::kPreparedStatementBetter;
        issue.desc = desc;
        issue.advice = "It is recommended to use SQLiteStatement optimization.";

        if(recently_reported_id == issue.id){
            sVerbose("PreparedStatementBetterChecker::PublishIssue recently reported %s", issue.id.c_str());
            return;
        }
        recently_reported_id = issue.id;
        issues->push_back(issue);
    }

    CheckScene PreparedStatementBetterChecker::GetCheckScene() {
        return CheckScene::kSample;
    }

    int PreparedStatementBetterChecker::GetSqlCntToSample() {
        return 30;
    }
}
