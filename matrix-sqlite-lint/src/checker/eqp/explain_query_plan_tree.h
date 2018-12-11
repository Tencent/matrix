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
// A tree data struct to construct the explain query plan result list,
// which can help analyze the plans.
//
// Author: liyongjie
// Created by liyongjie
//

#ifndef SQLITE_LINT_CHECKER_EQP_EXPLAIN_QUERY_PLAN_TREE_H
#define SQLITE_LINT_CHECKER_EQP_EXPLAIN_QUERY_PLAN_TREE_H

#include <core/lint_info.h>
#include <regex>

namespace sqlitelint {

    class EQPTreeNode {
    public:
        explicit EQPTreeNode(const Record& main_record);
        void AddChild(EQPTreeNode* child);
        const std::vector<EQPTreeNode*>& GetChilds() const ;

        const Record& GetMainRecord() const ;

        void AddRecordToGroup(const Record& record);
        const std::vector<Record>& GetGroupRecords() const;

    private:
        const Record& main_record_;
        std::vector<EQPTreeNode*> childs_;
        std::vector<Record> group_records_;
    };

    class ExplainQueryPlanTree {
    public:
        explicit ExplainQueryPlanTree(const QueryPlan& query_plan);
        ~ExplainQueryPlanTree();
        EQPTreeNode* GetRootNode();
        void DumpTree();
    private:
        static const std::regex kExtractSelectIdRgx;
        EQPTreeNode* root_node_;

        EQPTreeNode* BuildFantasyEQPTree(const std::vector<Record>& plans, int* start_index);
        void ParseCompoundRecord(const Record& record, std::vector<int>* sub_query_select_ids);
        void ReleaseTree(EQPTreeNode* node);
        void DoDumpTree(const EQPTreeNode *node, const int level, std::string* print);
    };
}

#endif //SQLITE_LINT_CHECKER_EQP_EXPLAIN_QUERY_PLAN_TREE_H
