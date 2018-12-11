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

#include "explain_query_plan_tree.h"
#include "comm/log/logger.h"
#include "core/lint_logic.h"

namespace sqlitelint {
    EQPTreeNode::EQPTreeNode(const Record& main_record)
            : main_record_(main_record) {
        group_records_.push_back(main_record);
    }

    void EQPTreeNode::AddChild(EQPTreeNode *child) {
        childs_.insert(childs_.begin(), child);
    }

    const std::vector<EQPTreeNode*>& EQPTreeNode::GetChilds() const {
        return childs_;
    }

    const Record& EQPTreeNode::GetMainRecord() const {
        return main_record_;
    }

    void EQPTreeNode::AddRecordToGroup(const Record &record) {
        group_records_.insert(group_records_.begin(), record);
    }

    const std::vector<Record>& EQPTreeNode::GetGroupRecords() const {
        return group_records_;
    }

    ExplainQueryPlanTree::ExplainQueryPlanTree(const QueryPlan &query_plan) {
        root_node_ = new EQPTreeNode(Record::kEmpty);
        const std::vector<Record>& plans =  query_plan.plans_;
        int* start_index = new int(plans.size() - 1);

        while (*start_index >= 0) {
            root_node_->AddChild( BuildFantasyEQPTree(plans, start_index) );
        }

    }

    const std::regex ExplainQueryPlanTree::kExtractSelectIdRgx = std::regex("COMPOUND SUBQUERIES ([\\d]+) AND ([\\d]+)[\\s\\S]*");

    EQPTreeNode* ExplainQueryPlanTree::GetRootNode() {
        return root_node_;
    }

    void ExplainQueryPlanTree::DumpTree() {
        std::string* print = new std::string;
        DoDumpTree(root_node_, 0, print);
        sDebug("ExplainQueryPlanTree::DumpTree:\n %s", print->c_str());
        delete print;
    }

    void ExplainQueryPlanTree::DoDumpTree(const EQPTreeNode *node, const int level,
                                          std::string *print) {
        if (node == nullptr) {
            return;
        }

        for (int i = 0; i < 4*level ; i++) {
            print->append(" ");
        }

        for (auto& r : node->GetGroupRecords()) {
            print->append(r.detail_).append(" ");
        }
        print->append("\n");

        for (auto& n : node->GetChilds()) {
            DoDumpTree(n, level + 1, print);
        }
    }

    EQPTreeNode* ExplainQueryPlanTree::BuildFantasyEQPTree(const std::vector<Record> &plans,
                                                   int *start_index) {
        if (*start_index < 0) {
            return nullptr;
        }

        int to_group_record_index = -1;
        const Record& record = plans[*start_index];
        while (plans[*start_index].isUseTempTreeExplainRecord()) {
            to_group_record_index = *start_index;
            (*start_index) --;

            if (*start_index < 0) {
                sError("ExplainQueryPlanTree::BuildFantasyEQPTree start_index invalid");
                DumpQueryPlans(plans);
                return nullptr;
            }
        }

        const Record& main_record = plans[*start_index];
        if (main_record.isCompoundExplainRecord()) {
            EQPTreeNode* compound_node = new EQPTreeNode(main_record);
            (*start_index) --;

            std::unique_ptr<std::vector<int>> sub_query_select_ids(new std::vector<int>);
            ParseCompoundRecord(main_record, sub_query_select_ids.get());
            for (int id : *sub_query_select_ids) {
                EQPTreeNode* child = BuildFantasyEQPTree(plans, start_index);
                compound_node->AddChild(child);
            }

            if (to_group_record_index >= 0) {
                compound_node->AddRecordToGroup(plans[to_group_record_index]);
            }

            return compound_node;
        } else if (main_record.order_ > 0) {
            const int loop_cnt = record.order_ + 1;
            EQPTreeNode* join_node = new EQPTreeNode(Record::kEmpty);

            if (to_group_record_index >= 0) {
                EQPTreeNode* use_tmp_tree_node = new EQPTreeNode(plans[to_group_record_index]);

                join_node->AddChild(use_tmp_tree_node);
            }

            for (int i=0 ; i < loop_cnt && *start_index >= 0; i++) {
                if (plans[*start_index].isUseTempTreeExplainRecord()) {

                    if (*start_index - 1 < 0) {
                        sError("ExplainQueryPlanTree::BuildFantasyEQPTree start_index invalid");
                        DumpQueryPlans(plans);
                        return nullptr;
                    }

                    EQPTreeNode* child = new EQPTreeNode(plans[*start_index - 1]);
                    child->AddRecordToGroup(plans[*start_index]);

                    (*start_index) -= 2;

                    join_node->AddChild(child);
                } else {
                    EQPTreeNode* child = new EQPTreeNode(plans[*start_index]);
                    join_node->AddChild(child);
                    (*start_index) --;
                }
            }

            auto it = join_node->GetChilds().end();
            do {
                it --;
                if (*it != nullptr && (*it)->GetMainRecord().isOneLoopSubQueryExplainRecord()) {
                    (*it)->AddChild(BuildFantasyEQPTree(plans, start_index));
                }
            } while (it != join_node->GetChilds().begin());

            return join_node;

        } else if (main_record.isOneLoopSubQueryExplainRecord()) {
            EQPTreeNode* sub_query_node = new EQPTreeNode(main_record);
            if (to_group_record_index >= 0) {
                sub_query_node->AddRecordToGroup(plans[to_group_record_index]);
            }

            (*start_index) --;

            sub_query_node->AddChild(BuildFantasyEQPTree(plans, start_index));

            return sub_query_node;

        } else {
            //scalar queries are now excluded TODO
            EQPTreeNode* node = new EQPTreeNode(main_record);
            if (to_group_record_index >= 0) {
                node->AddRecordToGroup(plans[to_group_record_index]);
            }

            (*start_index) --;

            return node;
        }
    }

    void ExplainQueryPlanTree::ParseCompoundRecord(const Record &record,
                                                   std::vector<int> *sub_query_select_ids) {
        std::smatch m;
        int selectId;
        if(regex_search(record.detail_, m, kExtractSelectIdRgx)) {
            for (int i=1;i<m.size();i++) {
                selectId = atoi(m[i].str().c_str());
                sVerbose("CompoundTreeNode.parse: add select id =%d", selectId);
                sub_query_select_ids->push_back(selectId);
            }
        } else {
            sError("CompoundTreeNode.parse: not found select id. detail=%s", record.detail_.c_str());
        }
    }

    ExplainQueryPlanTree::~ExplainQueryPlanTree() {
        ReleaseTree(root_node_);
    }

    void ExplainQueryPlanTree::ReleaseTree(EQPTreeNode *node) {
        if (node == nullptr) {
            return;
        }

        for (EQPTreeNode* n : node->GetChilds()) {
            ReleaseTree(n);
        }

        delete node;
    }
}
