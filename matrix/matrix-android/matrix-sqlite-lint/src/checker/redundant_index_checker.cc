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

#include "redundant_index_checker.h"
#include <algorithm>
#include <comm/lint_util.h>
#include "comm/log/logger.h"
#include "core/lint_logic.h"

namespace sqlitelint {

    CheckScene RedundantIndexChecker::GetCheckScene() {
        return CheckScene::kAfterInit;
    }

    void RedundantIndexChecker::Check(LintEnv &env, const SqlInfo &sql_info,
                                      std::vector<Issue> *issues) {
        std::vector<TableInfo> tables = env.GetTablesInfo();

        sVerbose("RedundantIndexChecker::Check tableInfoList size %zu",tables.size());

        for (const TableInfo& table_info : tables) {

            if (env.IsInWhiteList(kCheckerName, table_info.table_name_)) {
                sVerbose("RedundantIndexChecker::Check in white list:%s ", table_info.table_name_.c_str());
                continue;
            }

            std::vector<IndexInfo> indexs = table_info.indexs_;

            if (indexs.empty()) {
                continue;
            }

            std::sort(indexs.begin(), indexs.end(), RedundantIndexChecker::SortIndex);

            std::vector<RedundantIndexGroup>* groups = new std::vector<RedundantIndexGroup>;
            MakeDistinctGroup(indexs, groups);

            for (const RedundantIndexGroup& group : *groups) {
                sVerbose("RedundantIndexChecker::Check group index name: %s", group.GetMainIndex().index_name_.c_str());

                if (group.HasRedundantIndexs()) {
                    PublishIssue(env, table_info.table_name_, group, issues);
                }
            }

            delete groups;
        }
    }

    void RedundantIndexChecker::MakeDistinctGroup(const std::vector<IndexInfo> &indexs,
                                                  std::vector<RedundantIndexGroup> *groups) {
        if (indexs.empty()) {
            return;
        }

        std::vector<IndexInfo> rest_indexs;

        const IndexInfo& main_index = indexs[0];
        RedundantIndexGroup distinct_index_group(main_index);
        for (size_t i=1; i < indexs.size();i++) {
            // if not belong to this group
            // put it in the rest non-grouped vector,
            // and try to group in the next round of MakeDistinctGroup
            if (!distinct_index_group.Try2AddToGroup(indexs[i])) {
                rest_indexs.push_back(indexs[i]);
            }
        }

        groups->push_back(distinct_index_group);

        MakeDistinctGroup(rest_indexs, groups);
    }

    void RedundantIndexChecker::GetIndexColumnsString(const IndexInfo &index_info,
                                                      std::string *column_str) {
        size_t size = index_info.index_elements_.size();
        for (size_t i=0;i<size;i++) {
            if (i != 0) {
                column_str->append(",");
            }

            column_str->append(index_info.index_elements_[i].column_name_);
        }
    }

    bool RedundantIndexChecker::SortIndex(const IndexInfo &left, const IndexInfo &right) {
        if(left.index_elements_.size() < right.index_elements_.size()) {
            return false;
        } else if (left.index_elements_.size() > right.index_elements_.size()) {
            return true;
        } else {
            return left.seq_ > right.seq_;
        }
    }

    void RedundantIndexChecker::PublishIssue(const LintEnv& env, const std::string &table_name,
                                             const RedundantIndexGroup& group,
                                             std::vector<Issue> *issues) {
        const IndexInfo& main_index = group.GetMainIndex();
        std::string redundant_indexs_str;
        std::string* column_str = new std::string;
        for (const IndexInfo& index : group.GetRedundantIndexs()) {
            column_str->clear();
            GetIndexColumnsString(index, column_str);
            redundant_indexs_str.append(index.index_name_).append("(").append(*column_str).append(")");
        }
        delete column_str;

        std::string identity_info;
        identity_info.append("[").append(table_name).append("]")
                .append(main_index.index_name_).append(redundant_indexs_str);

        std::string* main_index_column_str = new std::string;
        GetIndexColumnsString(main_index, main_index_column_str);

        std::string desc;
        desc.append("Table(").append(table_name).append(") found redundant indexes:").append(main_index.index_name_)
                .append("(").append(*main_index_column_str).append(")").append("\n")
                .append(redundant_indexs_str);
        delete main_index_column_str;

        std::string advice;
        advice.append("You can keep index \"").append(main_index.index_name_).append("\", and delete others.");

        Issue issue;
        issue.id = GenIssueId(env.GetDbFileName(), kCheckerName, identity_info);
        issue.db_path = env.GetDbPath();
        issue.create_time = GetSysTimeMillisecond();
        issue.type = IssueType::kRedundantIndex;
        issue.table = table_name;
        issue.level = IssueLevel::kSuggestion;
        issue.desc = desc;
        issue.advice = advice;

        issues->push_back(issue);
    }

    RedundantIndexGroup::RedundantIndexGroup(const IndexInfo& main_index) : main_index_(main_index) {
    }

    bool RedundantIndexGroup::Try2AddToGroup(const IndexInfo &candidate) {
        sVerbose("RedundantIndexChecker::Check Try2AddToGroup %zu %zu", candidate.index_elements_.size(), main_index_.index_elements_.size());

        if (candidate.index_elements_.size() > main_index_.index_elements_.size()) {
            return false;
        }

        size_t i = 0;
        for (; i < candidate.index_elements_.size() ; i++) {
            if (candidate.index_elements_[i].column_index_ != main_index_.index_elements_[i].column_index_) {
                break;
            }
        }

        //TODO primary key judge
        //candidate' index is same with the current main index
        //Then the auto index has the highest priority
        //But if the main index is already a auto index then it's fixed; see main_index_lock_
        if (i == main_index_.index_elements_.size()) {
            // main index is locked, candidate is to be the redundant one
            if (main_index_lock_) {
                redundant_indexs_.push_back(candidate);
                return true;
            }

            if (IsSQLite3AutoIndex(main_index_.index_name_)) {
                redundant_indexs_.push_back(candidate);
                main_index_lock_ = true;
                return true;
            }

            if (IsSQLite3AutoIndex(candidate.index_name_)) {
                redundant_indexs_.push_back(main_index_);
                main_index_ = candidate;
                main_index_lock_ = true;
                return true;
            }

            redundant_indexs_.push_back(candidate);
            return true;

        // candidate is the prefix of the current index
        } else if (i == candidate.index_elements_.size()) {
            redundant_indexs_.push_back(candidate);
            return true;
        }

        return false;
    }

    const IndexInfo& RedundantIndexGroup::GetMainIndex() const{
        return main_index_;
    }

    const std::vector<IndexInfo>& RedundantIndexGroup::GetRedundantIndexs() const {
        return redundant_indexs_;
    }

    bool RedundantIndexGroup::HasRedundantIndexs() const {
        return !redundant_indexs_.empty();
    }
}
