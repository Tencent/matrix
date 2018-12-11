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
// The data structs exposed to SDK-Level; eg.android, iOS, windows
//
// Created by liyongjie on 2017/6/21
//

#ifndef SQLITE_LINT_SLINT_H
#define SQLITE_LINT_SLINT_H

#include <string>

namespace sqlitelint {

    // CheckerName can be used to configured the white list
    class CheckerName {
    public:
        static constexpr const char* const kExplainQueryPlanCheckerName = "ExplainQueryPlanChecker";
        static constexpr const char* const kAvoidAutoIncrementCheckerName = "AvoidAutoIncrementChecker";
        static constexpr const char* const kAvoidSelectAllCheckerName = "AvoidSelectAllChecker";
        static constexpr const char* const kWithoutRowIdBetterCheckerName = "WithoutRowIdBetterChecker";
        static constexpr const char* const kPreparedStatementBetterCheckerName = "PreparedStatementBetterChecker";
        static constexpr const char* const kRedundantIndexCheckerName = "RedundantIndexChecker";
    };

    // IssueLevel represents the scale of the issue
    // When the issue is over kSuggestion level, it's worth paying attention to
    typedef enum {
        kPass = 0,
        kTips,
        kSuggestion,
        kWarning,
        kError,
    } IssueLevel;

    typedef enum {
        kExplainQueryScanTable = 1,
        kExplainQueryUseTempTree,
        kExplainQueryTipsForLargerIndex,
        kAvoidAutoIncrement,
        kAvoidSelectAllChecker,
        kWithoutRowIdBetter,
        kPreparedStatementBetter,
        kRedundantIndex,
    } IssueType;

    // The information of the problem the checkers found
    // Issue will be published to the SDK level
    class Issue {
    public:
        std::string id;
        std::string db_path;
        IssueType type;
        IssueLevel level;
        std::string sql;
        std::string table;
        int64_t create_time;
        std::string desc;
        std::string detail;
        std::string advice;
        std::string ext_info;
        long sql_time_cost = 0;
        bool is_in_main_thread = false;
    };
}
#endif //SQLITE_LINT_SLINT_H
