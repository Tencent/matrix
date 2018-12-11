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
// Select * is not really recommended, due to the following reasons:
// 1. It perhaps cost more memory which differs from platforms
// 2. It may substantially reduce the chance to use the cover index if there has.
// And as we known, cover index will improve pretty well the peformance
// 3. Actually sqlite3 will finally expand the * to columns.
//
// This checker is to give you some tips when you do use "select *"
//
// Author : liyongjie
// Created by liyongjie
//

#ifndef SQLITE_LINT_CHECKER_AVOID_SELECT_ALL_CHECKER_H
#define SQLITE_LINT_CHECKER_AVOID_SELECT_ALL_CHECKER_H

#include <vector>
#include "checker/checker.h"

namespace sqlitelint {

    class AvoidSelectAllChecker : public Checker {
    public:
        virtual void Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues) override;
        virtual CheckScene GetCheckScene() override;
    private:
        constexpr static const char* const  kCheckerName = CheckerName::kAvoidSelectAllCheckerName;

        void PublishIssue(const LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues);
    };
}

#endif //SQLITE_LINT_CHECKER_AVOID_SELECT_ALL_CHECKER_H
