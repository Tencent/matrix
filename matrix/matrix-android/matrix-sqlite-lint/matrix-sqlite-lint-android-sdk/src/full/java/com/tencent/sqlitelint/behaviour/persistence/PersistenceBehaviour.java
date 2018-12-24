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

package com.tencent.sqlitelint.behaviour.persistence;

import com.tencent.sqlitelint.SQLiteLintIssue;
import com.tencent.sqlitelint.behaviour.BaseBehaviour;

import java.util.List;

/**
 * PersistenceBehaviour is fixed default behavior when issues published
 * And it behaves ahead of all the others.
 * It stores the issues, then the other behaviors can make use of the stored info.
 *
 * For example, alert behaviour can query the stored issues at any time {@link com.tencent.sqlitelint.behaviour.alert.CheckedDatabaseListActivity}.
 * If in need, the report behaviour can use the {@link IssueStorage} to avoid duplicated report
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/22.
 */

public class PersistenceBehaviour extends BaseBehaviour {

    @Override
    public void onPublish(List<SQLiteLintIssue> publishedIssues) {
        IssueStorage.saveIssues(publishedIssues);
    }

}
