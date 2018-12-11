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

package com.tencent.sqlitelint.behaviour;

import com.tencent.sqlitelint.IOnIssuePublishListener;

/**
 * The behaviour base.
 * A certain subclass defines what to do after finding a sql issue.
 *
 * {@link com.tencent.sqlitelint.SQLiteLintAndroidCore} use the IOnIssuePublishListener as a protocol
 * to dispatch the issue-published event.
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/15.
 */

public abstract class BaseBehaviour implements IOnIssuePublishListener {
    protected BaseBehaviour() {
    }
}
