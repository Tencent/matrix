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
//  receive and manage the executed sqls -> pre-process sql info
//  -> schedule the checkers -> output issues to the SDK level
//
//  checkers are scheduled to check in a single thread
//
//  author: liyongjie
//  Created by liyongjie on 2017/2/13.
//

#ifndef SQLITE_LINT_CORE_LINT_H
#define SQLITE_LINT_CORE_LINT_H

#include <condition_variable>
#include <deque>
#include <mutex>
#include <map>
#include <vector>
#include <thread>
#include "checker/checker.h"
#include "comm/lru_cache.h"
#include "core/lint_env.h"
#include "sqlite_lint.h"

namespace sqlitelint {

    class Lint {
    public:
        Lint(const char* db_path, OnPublishIssueCallback issued_callback);
        ~Lint();
        Lint(const Lint&) = delete;

        // collects executed or to checked sqls
        void NotifySqlExecution(const char *sql, const long time_cost, const char* ext_info);

        // the white list to avoid the checkers's mistake as much as possible
        void SetWhiteList(const std::map<std::string, std::set<std::string>>& white_list);

        void RegisterChecker(const std::string& checker_name);

        // enable checkers
		void RegisterChecker(Checker* checker);
    private:
        bool exit_;
        std::thread* check_thread_;
        std::thread* init_check_thread_;

        // A function pointer to callback to the SDK level
        const OnPublishIssueCallback issued_callback_;
        LintEnv env_;
        // The available(enabled) checkers
        std::map<CheckScene, std::vector<Checker*>> checkers_;
        // Manage the collected sqls in a queue; FIFO
        std::deque<std::unique_ptr<SqlInfo>> queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;
        // Use this to implement the unchecked logic
        LRUCache<std::string, bool> checked_sql_cache_;

        // Main schedule and check logic
        void Check();

        void InitCheck();

        // take to sql info to check
        // block if empty
        int TakeSqlInfo(std::unique_ptr<SqlInfo> &sql_info);

        // Pre process and get infos like sql-tree and so on
        // Checkers will need this pre-process info
        bool PreProcessSqlInfo(SqlInfo* sql_info);

        void PreProcessSqlString(std::string &sql);

        // Schedule the checkers of check_scene to process their check algorithm
        void ScheduleCheckers(const CheckScene check_scene, const SqlInfo& sql_info, std::vector<Issue>* published_issues);
    };
}
#endif /* SQLITE_LINT_CORE_LINT_H */
