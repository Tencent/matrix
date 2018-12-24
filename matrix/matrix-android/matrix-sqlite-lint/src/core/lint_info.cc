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

#include "lint_info.h"
#include "comm/lint_util.h"

namespace sqlitelint {
	void IndexInfo::AddIndexElement(IndexElement index_element) {
        std::vector<IndexElement>::iterator it = index_elements_.begin();
		for (; it != index_elements_.end(); it++) {
			if (index_element.pos_ < (*it).pos_) {
				break;
			}
		}
		if (it != index_elements_.end()) {
			index_elements_.insert(it, index_element);
		} else {
			index_elements_.push_back(index_element);
		}
	}

	SqlInfo::SqlInfo() {

	}
	SqlInfo::~SqlInfo() {
		Release();
	}

    void SqlInfo::CopyWithoutParse(SqlInfo &info){
        info = *this;
        info.parse_obj_ = nullptr;
    }

    void SqlInfo::Release(){
        if(nullptr != parse_obj_){
            sqlite3ParseDelete(parse_obj_);
            parse_obj_ = nullptr;
        }
    }

    Record::Record(const std::string& detail, const int select_id
            , const int order, const int from) : detail_(detail), select_id_(select_id)
            , order_(order), from_(from)  {
	}

	Record::~Record() {
	}

	const Record Record::kEmpty("", 0, 0, 0);

    bool Record::isCompoundExplainRecord() const {
        return strncmp(detail_.c_str(), Record::kEQPCompoundPrefix, strlen(Record::kEQPCompoundPrefix)) == 0;
    }

    bool Record::isOneLoopScanTableExplainRecord() const {
        return strncmp(detail_.c_str(), Record::kEQPOneLoopScanTablePrefix, strlen(Record::kEQPOneLoopScanTablePrefix)) == 0;
    }

    bool Record::isOneLoopSearchTableExplainRecord() const {
        return strncmp(detail_.c_str(), Record::kEQPOneLoopSearchTablePrefix, strlen(Record::kEQPOneLoopSearchTablePrefix)) == 0;
    }

    bool Record::isExecuteScalarExplainRecord() const {
        return strncmp(detail_.c_str(), Record::kEQPExecuteScalarPrefix, strlen(Record::kEQPExecuteScalarPrefix)) == 0;
    }

    bool Record::isOneLoopSubQueryExplainRecord() const {
        return strncmp(detail_.c_str(), Record::kEQPOneLoopScanSubqueryPrefix, strlen(Record::kEQPOneLoopScanSubqueryPrefix)) == 0
        || strncmp(detail_.c_str(), Record::kEQPOneLoopSearchSubqueryPrefix, strlen(Record::kEQPOneLoopSearchSubqueryPrefix)) == 0;
    }

    bool Record::isUseTempTreeExplainRecord() const {
        return strncmp(detail_.c_str(), Record::kEQPUseTempTreePrefix, strlen(Record::kEQPUseTempTreePrefix)) == 0;
    }
}
