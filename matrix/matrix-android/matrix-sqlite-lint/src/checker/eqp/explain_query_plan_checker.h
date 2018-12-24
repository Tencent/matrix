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
// Index is the most important feature of the SQLite3
// This checker is to help to you use index better, which may detect following index-using-issueso:
// 1. no indexs.
// 2. indexs not work
// 3. there may be a better composite (multi-column)
// The analyze algorithm is based on "explain query plan", short for eqp in the code
// Hopes help
//
// Author: liyongjie
// Created by liyongjie
//

#ifndef SQLITE_LINT_CHECKER_EQP_EXPLAIN_QUERY_PLAN_CHECKER_H
#define SQLITE_LINT_CHECKER_EQP_EXPLAIN_QUERY_PLAN_CHECKER_H

#include "checker/checker.h"
#include "explain_query_plan_tree.h"
#include "select_tree_helper.h"

namespace sqlitelint {

    class EQPCheckerEnv {
    public:
        EQPCheckerEnv(const SqlInfo* sql_info, LintEnv* env
                , SelectTreeHelper* tree_helper, QueryPlan*  query_plan
                , std::vector<Issue>* issues)
                : sql_info_(sql_info), env_(env)
                , tree_helper_(tree_helper), query_plan_(query_plan)
                , issues_(issues) {
        }

        const SqlInfo* const sql_info_;
        LintEnv* const env_;
        SelectTreeHelper* const tree_helper_;
        QueryPlan* const query_plan_;
        std::vector<Issue>* const issues_;
    };

    class ExplainQueryPlanChecker : public Checker {
    public:
        virtual void Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues) override;
        virtual CheckScene GetCheckScene() override;
    private:
        constexpr static const char* const kCheckerName = CheckerName::kExplainQueryPlanCheckerName;
        static const std::regex kExtractIndexRgx;
        static const std::regex kExtractTableRgx;
        static const std::regex kExtractAliasRgx;

        void WalkTreeAndCheck(const EQPTreeNode* node, const EQPTreeNode *parent, EQPCheckerEnv& eqp_checker_env);

        void JoinTableCheck(const EQPTreeNode* parent, EQPCheckerEnv& eqp_checker_env);

        void SingleTableCheck(const EQPTreeNode* node, EQPCheckerEnv& eqp_checker_env);

        // check if the indexs cover the columns occur in the where clause
        // if not, may be a larger composite index is better
        void LargerCompositeIndexCheck(const std::string& table, const std::string& table_alias
                , const std::string& detail, Select* select, EQPCheckerEnv& eqp_checker_env);

        void PublishIssue(const std::string& select_sql, const std::string& related_table, const IssueLevel& issue_level
                , const IssueType& issue_type, EQPCheckerEnv& eqp_checker_env);

        bool IsParamValid(const SqlInfo& sql_info);

        void ExtractTable(const std::string& detail, std::string& table);
        void ExtractAlias(const std::string& detail, std::string& alias);
        void ExtractIndex(const std::string& detail, std::string& index);
        void ExtractUsedIndex(const std::string& detail, std::string& index);
    };
}

#endif //SQLITE_LINT_CHECKER_EQP_EXPLAIN_QUERY_PLAN_CHECKER_H
