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
// Created by liyongjie on 2017/6/22
//

#ifndef SQLITE_LINT_CHECKER_CHECKER_H
#define SQLITE_LINT_CHECKER_CHECKER_H

#include <vector>
#include "core/lint_env.h"
#include "sqlite_lint.h"

namespace sqlitelint {

    typedef enum {
        kAfterInit,
        kSample,
        kUncheckedSql,
        kEverySql,
    } CheckScene;

    class Checker {
    public:
        virtual ~Checker();

        // TODO const SQLiteLintEnv& env
        // Check will be called according to the CheckScene of this checker
        // parameter env and sql_info will offers much info to the check algorithm
        // Output issues when check to find some places against the best practice of SQLite3 which this checker proposals
        virtual void Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues) = 0;

        // see enum CheckScene
        virtual CheckScene GetCheckScene() = 0;

        // Only the checker with CheckScene kSample, need override this method
        // And the return value will determine check rate
        // For example , if return 100, the checker will be scheduled to check every 100 sqls executed
        virtual int GetSqlCntToSample();
    };
}

#endif //SQLITE_LINT_CHECKER_CHECKER_H
