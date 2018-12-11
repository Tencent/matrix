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
// Created by liyongjie on 2017/6/21
//

#include <comm/log/logger.h>
#include "comm/lint_util.h"
#include "lint_logic.h"

namespace sqlitelint {

    std::string GenIssueId(const std::string& db_file_name, const std::string& checker_name, const std::string& identity_info) {
        return MD5(db_file_name + "_" + checker_name + "_" + identity_info);
    }

    bool IsSqlSupportCheck(const std::string& sql) {
        if (sql.length() < 6) {
            return false;
        }
        std::string keyword = sql.substr(0, 6);
        if (keyword == "select" || keyword == "insert" || keyword == "update"
            || keyword == "delete" || keyword == "replac") {
            return true;
        }
        return false;
    }

    bool IsSQLite3AutoIndex(const std::string& index) {
        return strncmp(index.c_str(), kAutoIndexPrefix, kAutoIndexPrefixLen) == 0;
    }

    void DumpQueryPlans(const std::vector<Record> & plans) {
        std::string print;
        for (auto & record : plans) {
            print.append(to_string(record.select_id_));
            print.append(to_string(record.order_));
            print.append(to_string(record.from_ ));
            print.append(record.detail_).append("\n");
        }
        sDebug("DumpQueryPlans :\n %s", print.c_str());
    }
}
