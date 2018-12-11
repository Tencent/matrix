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
// If a index A is a prefix of another index B, index A is redundant.
// And it may affect the space and performance
// This checker is to find such a issue
//
// Author: liyongjie
// Created by liyongjie
//

#ifndef SQLITE_LINT_CHECKER_REDUNDANT_INDEX_CHECKER_H
#define SQLITE_LINT_CHECKER_REDUNDANT_INDEX_CHECKER_H

#include "checker.h"

namespace sqlitelint {
    // A group of indexs;
    // And all the indexs in this group are the prefix of the main_index
    // That is to say, only the main_index_ is enough and the rest is redundant
    class RedundantIndexGroup {
    public:
        RedundantIndexGroup(const IndexInfo& main_index);

        // if the candidate is same with or the prefix of the current main_index_
        // it is belong to the group
        bool Try2AddToGroup(const IndexInfo& candidate);

        const IndexInfo& GetMainIndex() const;

        const std::vector<IndexInfo>& GetRedundantIndexs() const;

        bool HasRedundantIndexs() const;
    private:
        IndexInfo main_index_;
        bool main_index_lock_;
        std::vector<IndexInfo> redundant_indexs_;
    };

    class RedundantIndexChecker : public Checker {
    public:
        virtual void Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues) override;
        virtual CheckScene GetCheckScene() override;
    private:
        constexpr static const char* const  kCheckerName = CheckerName::kRedundantIndexCheckerName;

        void PublishIssue(const LintEnv& env, const std::string& table_name, const RedundantIndexGroup& group, std::vector<Issue>* issues);

        static bool SortIndex(const IndexInfo& left, const IndexInfo& right);
        //Use a recursion algorithm to make groups
        //See DistinctIndexGroup
        void MakeDistinctGroup(const std::vector<IndexInfo>& indexs, std::vector<RedundantIndexGroup>* groups);

        void GetIndexColumnsString(const IndexInfo& index_info, std::string* column_str);

    };
}

#endif //SQLITE_LINT_CHECKER_REDUNDANT_INDEX_CHECKER_H
