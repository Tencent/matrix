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

#include "white_list_mgr.h"
#include "comm/lint_util.h"
#include "comm/log/logger.h"

namespace sqlitelint {

    bool WhiteListMgr::IsInWhiteList(const std::string &checker_name, const std::string &target) const {
        const auto& it = white_list_.find(checker_name);
        if (it == white_list_.end()) {
            return false;
        }
        std::string low_case = target;
        ToLowerCase(low_case);
        if (it->second.find(low_case) == it->second.end()) {
            return false;
        }

        return true;
    }

    void WhiteListMgr::SetWhiteList(
            const std::map<std::string, std::set<std::string>> &white_list) {
        white_list_.clear();
        for (const auto& it: white_list) {
            white_list_[it.first] = std::set<std::string>();
            for (const auto& element : it.second) {
                std::string low_case_element = element;
                ToLowerCase(low_case_element);
                white_list_[it.first].insert(low_case_element);
            }
        }
    }
}
