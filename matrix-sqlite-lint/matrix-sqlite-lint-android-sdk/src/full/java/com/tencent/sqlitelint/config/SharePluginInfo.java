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

package com.tencent.sqlitelint.config;

/**
 * @author liyongjie
 *         Created by liyongjie on 2017/7/27.
 */

public class SharePluginInfo {

    public static final String TAG_PLUGIN = "SQLiteLint";

    public static final String ISSUE_KEY_ID = "id";
    public static final String ISSUE_KEY_DB_PATH = "dbPath";
    public static final String ISSUE_KEY_LEVEL = "level";
    public static final String ISSUE_KEY_TABLE = "table";
    public static final String ISSUE_KEY_SQL = "sql";
    public static final String ISSUE_KEY_DESC = "desc";
    public static final String ISSUE_KEY_DETAIL = "detail";
    public static final String ISSUE_KEY_ADVICE = "advice";
    public static final String ISSUE_KEY_CREATE_TIME = "createTime";
    public static final String ISSUE_KEY_STACK = "stack";
    public static final String ISSUE_KEY_SQL_TIME_COST = "sqlTimeCost";
    public static final String ISSUE_KEY_IS_IN_MAIN_THREAD = "isInMainThread";
}
