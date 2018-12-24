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


import android.database.sqlite.SQLiteDatabase;

import com.tencent.sqlitelint.SQLiteLint;

import java.util.ArrayList;
import java.util.List;

/**
 * // Temp
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/29.
 */

public final class SQLiteLintConfig {

    public SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode sqlExecutionCallbackMode) {
        if (sqlExecutionCallbackMode != null) {

        }
    }

    public void addConcernDB(ConcernDb concernDB) {
    }

    public List<ConcernDb> getConcernDbList() {
        return new ArrayList<>();
    }

    public static final class ConcernDb {
        public ConcernDb(SQLiteLint.InstallEnv installEnv, SQLiteLint.Options options) {
            if (installEnv != null && options != null) {

            }
        }

        public ConcernDb(SQLiteDatabase db) {
            if (db != null) {

            }
        }

        public ConcernDb setWhiteListXml(final int xmlResId) {
            return this;
        }

        public SQLiteLint.InstallEnv getInstallEnv() {
            return null;
        }

        public SQLiteLint.Options getOptions() {
            return null;
        }

        public int getWhiteListXmlResId() {
            return -1;
        }

        public ConcernDb enableAllCheckers() {
            return this;
        }

        public ConcernDb enableExplainQueryPlanChecker() {
            return this;
        }

        public ConcernDb enableAvoidSelectAllChecker() {
            return this;
        }

        public ConcernDb enableWithoutRowIdBetterChecker() {
            return this;
        }

        public ConcernDb enableAvoidAutoIncrementChecker() {
            return this;
        }

        public ConcernDb enablePreparedStatementBetterChecker() {
            return this;
        }

        public ConcernDb enableRedundantIndexChecker() {
            return this;
        }

        public List<String> getEnableCheckerList() {
            return new ArrayList<>();
        }

    }
}
