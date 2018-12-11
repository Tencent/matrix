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
// Using "Without Rowid" feature may sometimes bring space and performance advantages
// When the table meets the following condition, we advice you use "Without RowId" with the table:
// 1. has non-integer or composite (multi-column) PRIMARY KEYs
// 2. individual rows are not too large. The strict judgement algorithm now is that there's no text or blob type non-primary-key columns
// This checker works on advising the developer to use "Without Rowid" at a proper time.
// And publishes Tip-Level issues but it's recommended that you run tests to see if the "Without Rowid" helps.
// eg.
// CREATE TABLE IF NOT EXISTS wordcount(
//        word TEXT PRIMARY KEY,
//        cnt INTEGER
// ) WITHOUT ROWID;
//
// Author: liyongjie
// Created by liyongjie
//

#ifndef SQLITE_LINT_CHECKER_WITHOUT_ROWID_BETTER_CHECKER_H
#define SQLITE_LINT_CHECKER_WITHOUT_ROWID_BETTER_CHECKER_H

#include <vector>
#include "checker.h"

namespace sqlitelint {

    class WithoutRowIdBetterChecker : public Checker {
    public:
        virtual void Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues) override;
        virtual CheckScene GetCheckScene() override;

    private:

        constexpr static const char* const  kCheckerName = CheckerName::kWithoutRowIdBetterCheckerName;
        constexpr static const char* const  kWithoutRowIdKeyWord = "without rowid";

        void PublishIssue(const LintEnv& env, const std::string& table_name, std::vector<Issue>* issues);
        bool IsWithoutRowIdBetter(const TableInfo& table_info);
    };

}

#endif //SQLITE_LINT_CHECKER_WITHOUT_ROWID_BETTER_CHECKER_H
