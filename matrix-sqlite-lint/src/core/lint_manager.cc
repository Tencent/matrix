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

#include "lint_manager.h"

#include "lint.h"
#include "comm/log/logger.h"

namespace sqlitelint {
    std::mutex LintManager::lints_mutex_;
    LintManager* LintManager::instance_ = nullptr;

    LintManager* LintManager::Get() {
        if (!instance_){
            std::unique_lock<std::mutex> lock(lints_mutex_);
            if (!instance_){
                instance_ = new LintManager();
            }
            lock.unlock();
        }
        return instance_;
    }

    void LintManager::Release() {
        if (instance_){
            delete instance_;
            instance_ = nullptr;
        }
    }

    void LintManager::Install(const char* db_path, OnPublishIssueCallback issued_callback) {
        sInfo("LintManager::Install dbPath:%s", db_path);
        std::unique_lock<std::mutex> lock(lints_mutex_);
        std::map<const std::string, Lint*>::iterator it = lints_.find(db_path);
        if (it != lints_.end()) {
            lock.unlock();
            sWarn("Install already installed; dbPath: %s", db_path);
            return;
        }

        Lint* lint = new Lint(db_path, issued_callback);
        lints_.insert(std::pair<const std::string, Lint*>(db_path, lint));
        lock.unlock();
    }


    void LintManager::Uninstall(const std::string& db_path) {
        sInfo("uninstall path:%s", db_path.c_str());
        std::unique_lock<std::mutex> lock(lints_mutex_);
        std::map<const std::string, Lint*>::iterator it = lints_.find(db_path);
        if (it == lints_.end()) {
            lock.unlock();
            sWarn("NotifySqlExecution lint not installed; dbPath: %s", db_path.c_str());
            return;
        }

        Lint* lint = it->second;
        lints_.erase(it);
        delete lint;
        lock.unlock();
    }

    //TODO
    void LintManager::UninstallAll() {}

    void LintManager::NotifySqlExecution(const char *db_path, const char *sql, long time_cost, const char* ext_info) {
        std::unique_lock<std::mutex> lock(lints_mutex_);
        std::map<const std::string, Lint*>::iterator it = lints_.find(db_path);
        if (it == lints_.end()) {
            lock.unlock();
            sWarn("LintManager::NotifySqlExecution lint not installed; dbPath: %s", db_path);
            return;
        }
        it->second->NotifySqlExecution(sql, time_cost, ext_info);
        lock.unlock();
    }

    void LintManager::SetWhiteList(const char *db_path,
                                   const std::map<std::string, std::set<std::string>> &white_list) {
        std::unique_lock<std::mutex> lock(lints_mutex_);
        std::map<const std::string, Lint*>::iterator it = lints_.find(db_path);
        if (it == lints_.end()) {
            lock.unlock();
            sWarn("LintManager::SetWhiteList lint not installed; dbPath: %s", db_path);
            return;
        }
        it->second->SetWhiteList(white_list);
        lock.unlock();
    }

    void LintManager::EnableChecker(const char *db_path, const std::string &checker_name) {
        std::unique_lock<std::mutex> lock(lints_mutex_);
        std::map<const std::string, Lint*>::iterator it = lints_.find(db_path);
        if (it == lints_.end()) {
            lock.unlock();
            sWarn("LintManager::EnableChecker lint not installed; dbPath: %s", db_path);
            return;
        }
        it->second->RegisterChecker(checker_name);
        lock.unlock();
    }
}
