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

package com.tencent.sqlitelint;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;

import java.util.List;

/**
 * User interfaces to use SQLiteLint.
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/13.
 */

public class SQLiteLint {

    private SQLiteLint() {
    }
    public static void setSqlExecutionCallbackMode(SqlExecutionCallbackMode sqlExecutionCallbackMode) {
    }
    public static SqlExecutionCallbackMode getSqlExecutionCallbackMode() {
        return SqlExecutionCallbackMode.HOOK;
    }

    public static void install(Context context, SQLiteDatabase db) {
    }

    public static void install(Context context, InstallEnv installEnv, Options options) {
    }

    public static void notifySqlExecution(String concernedDbPath, String sql, int executeTime) {
    }

    public static void uninstall(String concernedDbPath) {
    }

    public static void setWhiteList(String concernedDbPath, final int xmlResId) {
    }

    public static void enableCheckers(String concernedDbPath, final List<String> enableCheckerList) {
    }

    public static final class InstallEnv {

        private final String mConcernedDbPath;
        private final ISQLiteExecutionDelegate mSQLiteExecutionDelegate;

        public InstallEnv(String concernedDbPath, ISQLiteExecutionDelegate executionDelegate) {
            assert concernedDbPath != null;
            assert executionDelegate != null;

            mConcernedDbPath = concernedDbPath;
            mSQLiteExecutionDelegate = executionDelegate;
        }

        public String getConcernedDbPath() {
            return mConcernedDbPath;
        }

        public ISQLiteExecutionDelegate getSQLiteExecutionDelegate() {
            return mSQLiteExecutionDelegate;
        }
    }

    public enum SqlExecutionCallbackMode {
        HOOK,
        CUSTOM_NOTIFY,
    }

    private static final int BEHAVIOUR_ALERT = 0x1;
    private static final int BEHAVIOUR_REPORT = 0x2;

    /**
     * You can use options to employ SQLiteLint flexibly.
     */
    public static final class Options {
        public static final Options LAX = new Builder().build();

        private int behaviourMask;

        /**
         * it's default enable if you don't call {@link Builder#setAlertBehaviour(boolean)}
         * @return
         */
        public boolean isAlertBehaviourEnable() {
            return (behaviourMask & BEHAVIOUR_ALERT) > 0;

        }

        /**
         * it's default disable if you don't call {@link Builder#setReportBehaviour(boolean)}
         * @return
         */
        public boolean isReportBehaviourEnable() {
            return (behaviourMask & BEHAVIOUR_REPORT) > 0;
        }

        public static final class Builder {
            private int mBehaviourMask;

            public Builder() {
                /* enable the alert behaviour as default*/
                mBehaviourMask |= BEHAVIOUR_ALERT;
            }

            /**
             * TODO
             * @param enable
             * @return
             */
            public Builder setAlertBehaviour(boolean enable) {
                if (enable) {
                    mBehaviourMask |= BEHAVIOUR_ALERT;
                } else {
                    mBehaviourMask &= ~BEHAVIOUR_ALERT;
                }

                return this;
            }

            /**
             * TODO
             * @param enable
             * @return
             */
            public Builder setReportBehaviour(boolean enable) {
                if (enable) {
                    mBehaviourMask |= BEHAVIOUR_REPORT;
                } else {
                    mBehaviourMask &= ~BEHAVIOUR_REPORT;
                }

                return this;
            }

            public Options build() {
                Options options =  new Options();
                options.behaviourMask = mBehaviourMask;
                return options;
            }
        }
    }
}
