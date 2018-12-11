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

#include "explain_query_plan_checker.h"
#include "comm/log/logger.h"
#include "comm/lint_util.h"
#include "core/lint_logic.h"
#include "core/sql_Info_processor.h"
#include "where_clause_table_column_finder.h"


namespace sqlitelint {
    //TODO primary key
    const std::regex ExplainQueryPlanChecker::kExtractIndexRgx = std::regex("(?:SEARCH) .+? (?:USING.*?INDEX) ([^ ]+)");
    const std::regex ExplainQueryPlanChecker::kExtractTableRgx = std::regex("(?:SCAN|SEARCH) TABLE ([^ ]+)");
    const std::regex ExplainQueryPlanChecker::kExtractAliasRgx = std::regex("(?:SCAN|SEARCH) TABLE .+? AS ([^ ]+)");

    CheckScene ExplainQueryPlanChecker::GetCheckScene() {
        return CheckScene::kUncheckedSql;
    }

    void ExplainQueryPlanChecker::Check(LintEnv &env, const SqlInfo &sql_info,
                                        std::vector<Issue> *issues) {
        const std::string& sql = sql_info.sql_;
        const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;
        sVerbose("ExplainQueryPlanChecker::Check sql: %s, whildcard_sql: %s", sql.c_str(), wildcard_sql.c_str());

        if (!IsParamValid(sql_info)) {
            return;
        }

        if (env.IsInWhiteList(kCheckerName, sql) || env.IsInWhiteList(kCheckerName, wildcard_sql)) {
            sVerbose("ExplainQueryPlanChecker::Check in white list");
            return;
        }

        std::unique_ptr<QueryPlan> query_plan(new QueryPlan);
        int ret = env.GetExplainQueryPlan(sql, query_plan.get());
        if (ret != 0) {
            sError("ExplainQueryPlanChecker::Check GetExplainQueryPlan failed; ret: %d", ret);
            return;
        }

        DumpQueryPlans(query_plan->plans_);

        std::unique_ptr<ExplainQueryPlanTree> eqp_tree(new ExplainQueryPlanTree(*query_plan));
        eqp_tree->DumpTree();

        EQPTreeNode* root_node = eqp_tree->GetRootNode();
        std::unique_ptr<SelectTreeHelper> select_tree_helper(new SelectTreeHelper(sql_info.parse_obj_->parsed.array[0].result.selectObj));
        select_tree_helper->Process();

        EQPCheckerEnv eqp_checker_env(&sql_info, &env, select_tree_helper.get(), query_plan.get(), issues);

        WalkTreeAndCheck(root_node, nullptr, eqp_checker_env);
    }

    void ExplainQueryPlanChecker::WalkTreeAndCheck(const EQPTreeNode *node,
                                                   const EQPTreeNode *parent,
                                                   EQPCheckerEnv& eqp_checker_env) {
        if (node == nullptr) {
            return;
        }

        // only check the leaf node
        if (node->GetChilds().empty()) {
            if (parent == nullptr) {
                return;
            }
            // if the last(from left to right) child's plan's order > 0, this is a join loop
            // analyze them together once
            int child_cnt = parent->GetChilds().size();
            int last_child_plan_order = (child_cnt > 1 ? (parent->GetChilds()[child_cnt - 1]->GetMainRecord().order_) : 0);
            sVerbose("ExplainQueryPlanChecker::WalkTreeAndCheck leaf brothers_cnt: %d, last_child_order: %d", child_cnt, last_child_plan_order);
            if (last_child_plan_order > 0 ) {
                if (node->GetMainRecord().order_ == last_child_plan_order) {
                    JoinTableCheck(parent, eqp_checker_env);
                }
            // order 0 means single table scan
            } else {
                SingleTableCheck(node, eqp_checker_env);
            }
        }

        for (auto child : node->GetChilds()) {
            WalkTreeAndCheck(child, node, eqp_checker_env);
        }

    }

    void ExplainQueryPlanChecker::SingleTableCheck(const EQPTreeNode *node,
                                                   EQPCheckerEnv &eqp_checker_env) {
        sVerbose("ExplainQueryPlanChecker::SingleTableCheck");

        const Record& main_record = node->GetMainRecord();

        std::string table;
        ExtractTable(main_record.detail_, table);
        if (table.empty()) {
            return;
        }

        if (eqp_checker_env.env_->IsInWhiteList(kCheckerName, table)) {
            sVerbose("ExplainQueryPlanChecker::SingleTableCheck in white list; table: %s", table.c_str());
            return;
        }

        SelectTreeHelper* tree_helper = eqp_checker_env.tree_helper_;
        Select* select = tree_helper->GetSelect(table);
        if (select == nullptr) {
            sError("ExplainQueryPlanChecker::SingleTableCheck getSelect null, table: %s", table.c_str());
            return;
        }

        SqlInfoProcessor sql_info_processor;
        const std::string select_sql = sql_info_processor.GetSql(select, true);

        if (eqp_checker_env.env_->IsInWhiteList(kCheckerName, select_sql)) {
            sVerbose("ExplainQueryPlanChecker::SingleTableCheck in white list; select sql: %s", select_sql.c_str());
            return;
        }

        for (const Record& record : node->GetGroupRecords()) {
            if (record.isUseTempTreeExplainRecord()) {
                PublishIssue(select_sql, table, IssueLevel::kSuggestion, IssueType::kExplainQueryUseTempTree, eqp_checker_env);
            } else {
                if (select->pWhere || select->pHaving) {
                    //TODO 如果where子句是恒真的情况呢?
                    if (record.isOneLoopScanTableExplainRecord()) {
                        PublishIssue(select_sql, table, IssueLevel::kSuggestion, IssueType::kExplainQueryScanTable, eqp_checker_env);
                    } else if (record.isOneLoopSearchTableExplainRecord()) {
                        std::string alias;//commonly only once in this for loop
                        ExtractAlias(main_record.detail_, alias);
                        LargerCompositeIndexCheck(table, alias, main_record.detail_, select, eqp_checker_env);
                    }
                }
            }
        }
    }

    void ExplainQueryPlanChecker::JoinTableCheck(const EQPTreeNode *parent, EQPCheckerEnv& eqp_checker_env) {
        sVerbose("ExplainQueryPlanChecker::JoinTableCheck");

        SqlInfoProcessor sql_info_processor;
        SelectTreeHelper* tree_helper = eqp_checker_env.tree_helper_;
        LintEnv* env = eqp_checker_env.env_;

        EQPTreeNode* child;
        for (size_t i = 0; i < parent->GetChilds().size(); i++) {
            child = parent->GetChilds()[i];
            const Record& main_record = child->GetMainRecord();

            if (main_record.isOneLoopSubQueryExplainRecord()) {
                continue;
            }

            std::string table;
            std::string alias;

            ExtractTable(main_record.detail_, table);
            ExtractAlias(main_record.detail_, alias);

            //TODO
            if (table.empty()/* || alias->empty()*/) {
                continue;
            }

            if (env->IsInWhiteList(kCheckerName, table)) {
                sVerbose("ExplainQueryPlanChecker::JoinTableCheck in white list; table:%s", table.c_str());
                continue;
            }

            //TODO
            Select* select = tree_helper->GetSelect(table);
            if (select == nullptr) {
                sError("ExplainQueryPlanChecker::JoinTableCheck getSelect null, table: %s", table.c_str());
                continue;
            }

            const std::string select_sql = sql_info_processor.GetSql(select, true);
            if (env->IsInWhiteList(kCheckerName, select_sql)) {
                sVerbose("ExplainQueryPlanChecker::JoinTableCheck in white list; select_sql:%s", select_sql.c_str());
                continue;
            }
            TableInfo table_info;
            env->GetTableInfo(table,table_info);

            if (table_info.table_name_.empty()) {
                sWarn("ExplainQueryPlanChecker::JoinTableCheck table_info empty");
                return;
            }

            WhereClauseTableColumnFinder finder(select->pWhere, table, alias, table_info.columns_);

            if (!finder.HasTargetTableColumnInWhereClause()) {
                continue;
            }

            if (!finder.IsTargetTableHasConstantExpression() && i == 0) {
                continue;
            }

            for (const auto& record : child->GetGroupRecords()) {
                //USE TEMP B-TREE issue
                if (record.isUseTempTreeExplainRecord()) {
                    PublishIssue(select_sql, table, IssueLevel::kSuggestion, IssueType::kExplainQueryUseTempTree, eqp_checker_env);
                } else {
                    // SCAN Table issue
                    if (record.isOneLoopScanTableExplainRecord()) {
                        PublishIssue(select_sql, table, IssueLevel::kSuggestion, IssueType::kExplainQueryScanTable, eqp_checker_env);
                    } else if (record.isOneLoopSearchTableExplainRecord()) {
                        LargerCompositeIndexCheck(table, alias, main_record.detail_, select, eqp_checker_env);
                    }
                }
            }
        }
    }

    void ExplainQueryPlanChecker::LargerCompositeIndexCheck(const std::string &table,
                                                            const std::string &table_alias,
                                                            const std::string &detail,
                                                            Select *select,
                                                            EQPCheckerEnv &eqp_checker_env) {
        sVerbose("ExplainQueryPlanChecker::LargerCompositeIndexCheck table: %s", table.c_str());

        LintEnv* env = eqp_checker_env.env_;

        SqlInfoProcessor sql_info_processor;
        const std::string select_sql = sql_info_processor.GetSql(select, true);
        if (env->IsInWhiteList(kCheckerName, select_sql)) {
            sVerbose("ExplainQueryPlanChecker::LargerCompositeIndexCheck in white list; select_sql: %s", select_sql.c_str());
            return;
        }

        std::string index_name;
        ExtractIndex(detail, index_name);

        TableInfo table_info;
        env->GetTableInfo(table,table_info);

        if (table_info.table_name_.empty()) {
            sWarn("ExplainQueryPlanChecker::LargerCompositeIndexCheck table_info empty");
            return;
        }

        if (env->IsInWhiteList(kCheckerName, table_info.table_name_)) {
            sVerbose("ExplainQueryPlanChecker::LargerCompositeIndexCheck in white list; table: %s", table_info.table_name_.c_str());
            return;
        }

        const std::vector<IndexInfo>& indexs = table_info.indexs_;

        Expr* target = select->pWhere ? select->pWhere : select->pHaving;
        WhereClauseTableColumnFinder finder(target, table, table_alias, table_info.columns_);

        const std::vector<std::string>& where_clause_columns = finder.GetTargetTableColumnsInWhereClause();

        if (where_clause_columns.empty()) {
            return;
        }

        if (where_clause_columns.size() > 3) {
            return;
        }

        std::set<std::string> already_cover_columns;
        std::string used_index_name;
        ExtractUsedIndex(detail, used_index_name);
        sVerbose("ExplainQueryPlanChecker::LargerCompositeIndexCheck used_index_name %s", used_index_name.c_str());

        if (index_name.empty()) {
            sDebug("ExplainQueryPlanChecker::LargerCompositeIndexCheck index_name empty");
            if (used_index_name.length() == 0) {
                sDebug("ExplainQueryPlanChecker::LargerCompositeIndexCheck used_index_name empty");
                return;
            }
            for (const auto &where_clause_column : where_clause_columns) {
                if (used_index_name.find(where_clause_column) == -1) {
                    PublishIssue(select_sql, table, IssueLevel::kTips, IssueType::kExplainQueryTipsForLargerIndex, eqp_checker_env);
                    break;
                }
            }
            return;
        }

        if (indexs.empty()) {
            sDebug("ExplainQueryPlanChecker::LargerCompositeIndexCheck indexs empty");
            return;
        }

        for (const auto& index : indexs) {
            if (CompareIgnoreCase(index.index_name_, index_name) == 0) {
                for (const auto& index_element : index.index_elements_) {
                    //not really used
                    if (used_index_name.length() > 0 && used_index_name.find(index_element.column_name_) == -1) {
                        continue;
                    }
                    already_cover_columns.insert(index_element.column_name_);
                }
            }
        }

        bool if_all_where_clause_columns_covered = true;
        for (const auto& where_clause_column : where_clause_columns) {
            if (already_cover_columns.find(where_clause_column) == already_cover_columns.end()) {
                if_all_where_clause_columns_covered = false;
                break;
            }
        }

        if (!if_all_where_clause_columns_covered) {
            PublishIssue(select_sql, table, IssueLevel::kTips, IssueType::kExplainQueryTipsForLargerIndex, eqp_checker_env);
        }
    }

    void ExplainQueryPlanChecker::PublishIssue(const std::string &select_sql, const std::string& related_table,
                                               const IssueLevel &issue_level, const IssueType &issue_type,
                                               EQPCheckerEnv& eqp_checker_env) {
        LintEnv* env = eqp_checker_env.env_;
        QueryPlan* query_plan = eqp_checker_env.query_plan_;

        std::string identity_info;
        const std::string& wildcard_sql = eqp_checker_env.sql_info_->wildcard_sql_.empty()
                                         ? eqp_checker_env.sql_info_->sql_ : eqp_checker_env.sql_info_->wildcard_sql_;
        identity_info.append("EQP-Check[").append(wildcard_sql)
                .append("][").append(to_string(issue_type)).append("]");

        std::string query_plans_str;
        for (const auto& r : query_plan->plans_) {
            query_plans_str.append(r.detail_).append("\n");
        }

        std::string defective_select_desc;
        switch (issue_type) {
            case IssueType::kExplainQueryScanTable:
                defective_select_desc.append("Among them, the sub query \"").append(select_sql).append("\" did not use index lead to scan table.");
                break;
            case IssueType::kExplainQueryUseTempTree:
                defective_select_desc.append("Among them, the sub query \"").append(select_sql).append("\" use temp b-tree to sort, you can use index to optimize.");
                break;
            case IssueType::kExplainQueryTipsForLargerIndex:
                defective_select_desc.append("Among them, the sub query \"").append(select_sql).append("\" it's recommended to use multi-column combination index to optimize if poor performance found.");
                break;
            default:
                break;
        }

        std::string detail;
        detail.append("Query plan is as follows:\n").append(query_plans_str).append("\n").append(defective_select_desc);

        std::string desc;
        desc.append("The following sql can use index optimization:\n");
        desc.append(eqp_checker_env.sql_info_->sql_);

        const std::string db_file_name = env->GetDbFileName();

        Issue issue;
        issue.id = GenIssueId(db_file_name, kCheckerName, identity_info);
        issue.db_path = env->GetDbPath();
        issue.create_time = GetSysTimeMillisecond();
        issue.level = issue_level;
        issue.type = issue_type;
        issue.sql = eqp_checker_env.sql_info_->sql_;
        issue.table = related_table;
        issue.desc = desc;
        issue.detail = detail;
        issue.ext_info = eqp_checker_env.sql_info_->ext_info_;
        issue.sql_time_cost = eqp_checker_env.sql_info_->time_cost_;
        issue.is_in_main_thread = eqp_checker_env.sql_info_->is_in_main_thread_;

        eqp_checker_env.issues_->push_back(issue);
    }

    bool ExplainQueryPlanChecker::IsParamValid(const SqlInfo &sql_info) {
        if (sql_info.parse_obj_ == nullptr) {
            sError("ExplainQueryPlanChecker::IsParamValid parseObj null");
            return false;
        }

        if (sql_info.parse_obj_->parsed.array == nullptr) {
            sError("ExplainQueryPlanChecker::IsParamValid parseObj->parsed.array null");
            return false;
        }

        if (sql_info.parse_obj_->parsed.array[0].sqltype != SQLTYPE_SELECT) {
            return false;
        }

        if (!sql_info.parse_obj_->parsed.array[0].result.selectObj) {
            sError("ExplainQueryPlanChecker::IsParamValid parseObj->parsed.array[0].result.selectObj null");
            return false;
        }

        return true;
    }

    void ExplainQueryPlanChecker::ExtractTable(const std::string &detail, std::string &table) {
        std::smatch m;
        if (regex_search(detail, m, kExtractTableRgx)) {
            table.assign(m[1]);
            return;
        }
    }

    void ExplainQueryPlanChecker::ExtractAlias(const std::string &detail, std::string &alias) {
        std::smatch m;
        if (regex_search(detail, m, kExtractAliasRgx)) {
            alias.assign(m[1]);
            return;
        }
    }

    void ExplainQueryPlanChecker::ExtractIndex(const std::string &detail, std::string &index) {
        std::smatch m;
        if (regex_search(detail, m, kExtractIndexRgx)) {
            index.assign(m[1]);
            return;
        }
    }

    void ExplainQueryPlanChecker::ExtractUsedIndex(const std::string &detail, std::string &index) {
        int left = detail.find('(');
        int right = detail.find(')');
        if(right > left && left >=0) {
            index = detail.substr(left + 1, right - left - 1);
        }
    }
}
