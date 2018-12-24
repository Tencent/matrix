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
// if the concerned target is in white list, checker will not publish issues
// one db one lint one white list manager
//
// Author: liyongjie
// Created by liyongjie
//

#ifndef SQLITE_LINT_CORE_LINT_WHITE_LIST_H
#define SQLITE_LINT_CORE_LINT_WHITE_LIST_H

#include <string>
#include <set>
#include <map>

namespace sqlitelint {

    class WhiteListMgr {
    public:
        bool IsInWhiteList(const std::string& checker_name, const std::string& target) const;
        void SetWhiteList(const std::map<std::string, std::set<std::string>>& white_list);
    private:
        std::map<std::string, std::set<std::string>> white_list_;
    };
}

#endif //SQLITE_LINT_CORE_LINT_WHITE_LIST_H
