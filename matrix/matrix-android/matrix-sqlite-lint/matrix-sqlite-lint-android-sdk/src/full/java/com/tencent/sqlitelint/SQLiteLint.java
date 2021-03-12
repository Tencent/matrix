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

import com.tencent.sqlitelint.behaviour.report.IssueReportBehaviour;
import com.tencent.sqlitelint.util.SLog;
import com.tencent.sqlitelint.util.SQLite3ProfileHooker;

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

    public static void init() {
        SQLiteLintNativeBridge.loadLibrary();
    }

    private static SqlExecutionCallbackMode sSqlExecutionCallbackMode = null;

    /**
     * 1. It's a must step to use SQLiteLint which define how to collects executed sqls.
     * {@link #install(Context, SQLiteDatabase)} and {@link #install(Context, InstallEnv, Options)}
     * 2. It can't be changed after it's first assignment
     * 3. sSqlExecutionCallbackMode is shared in a single process
     * 4. if using {@link SqlExecutionCallbackMode#HOOK} mode, you must setSqlExecutionCallbackMode to HOOK before open the database
     *
     * @see SqlExecutionCallbackMode
     *
     * @param sqlExecutionCallbackMode
     */
    public static void setSqlExecutionCallbackMode(SqlExecutionCallbackMode sqlExecutionCallbackMode) {
        if (sSqlExecutionCallbackMode != null) {
            return;
        }

        sSqlExecutionCallbackMode = sqlExecutionCallbackMode;
        if (sSqlExecutionCallbackMode == SqlExecutionCallbackMode.HOOK) {
            // hook must called before open the database
            SQLite3ProfileHooker.hook();
        }
    }

    public static SqlExecutionCallbackMode getSqlExecutionCallbackMode() {
       return sSqlExecutionCallbackMode;
    }

    public static void install(Context context, SQLiteDatabase db) {
        assert db != null;
        assert sSqlExecutionCallbackMode != null
                : "SqlExecutionCallbackMode not set！setSqlExecutionCallbackMode must be called before install";

        InstallEnv installEnv = new InstallEnv(db.getPath(), new SimpleSQLiteExecutionDelegate(db));
        SQLiteLintAndroidCoreManager.INSTANCE.install(context, installEnv, Options.LAX);
    }

    /**
     * A custom installation.
     *
     * TODO example
     *
     * @param context
     * @param installEnv You must correctly defines this variable, it's the key point to start up the SQLiteLint.
     *                  Give a little effort to read the {@link InstallEnv} and the example above.
     * @param options
     */
    public static void install(Context context, InstallEnv installEnv, Options options) {
        assert installEnv != null;
        assert sSqlExecutionCallbackMode != null
                : "SqlExecutionCallbackMode is UNKNOWN！setSqlExecutionCallbackMode must be called before install";

        options = (options == null) ? Options.LAX : options;

        SQLiteLintAndroidCoreManager.INSTANCE.install(context, installEnv, options);
    }

    /**
     * Notify the sql execution manually when you enable the {@link SqlExecutionCallbackMode#CUSTOM_NOTIFY}
     * @see #install(Context, InstallEnv, Options)
     */
    public static void notifySqlExecution(String concernedDbPath, String sql, int timeCost) {
        if (SQLiteLintAndroidCoreManager.INSTANCE.get(concernedDbPath) == null) {
            return;
        }

        SQLiteLintAndroidCoreManager.INSTANCE.get(concernedDbPath).notifySqlExecution(concernedDbPath, sql, timeCost);
    }

    /**
     *
     * @param concernedDbPath the dbPath explicitly passed in installEnv {@link #install(Context, InstallEnv, Options)}
     *                        or implicitly passed in db (db.getPath) {@link #install(Context, SQLiteDatabase)}
     */
    public static void uninstall(String concernedDbPath) {
        SQLiteLintAndroidCoreManager.INSTANCE.get(concernedDbPath).release();
        SQLiteLintAndroidCoreManager.INSTANCE.remove(concernedDbPath);
    }

    public static void setWhiteList(String concernedDbPath, final int xmlResId) {
        if (SQLiteLintAndroidCoreManager.INSTANCE.get(concernedDbPath) == null) {
            return;
        }

        SQLiteLintAndroidCoreManager.INSTANCE.get(concernedDbPath).setWhiteList(xmlResId);
    }

    public static void enableCheckers(String concernedDbPath, final List<String> enableCheckerList) {
        if (SQLiteLintAndroidCoreManager.INSTANCE.get(concernedDbPath) == null) {
            return;
        }

        if (enableCheckerList == null || enableCheckerList.isEmpty()) {
            return;
        }

        SQLiteLintAndroidCoreManager.INSTANCE.get(concernedDbPath).enableCheckers(enableCheckerList);
    }

    static IssueReportBehaviour.IReportDelegate sReportDelegate;
    static void setReportDelegate(IssueReportBehaviour.IReportDelegate reportDelegate) {
        sReportDelegate = reportDelegate;
    }

    public static String sPackageName = null;
    public static void setPackageName(Context context) {
        if (sPackageName == null) {
            sPackageName = context.getPackageName();
        }
    }

    /**
     * A data struct of some variables which guide the installation and make SQLiteLint work.
     * Used in {@link #install(Context, InstallEnv, Options)}
     */
    public static final class InstallEnv {

        private final String mConcernedDbPath;
        private final ISQLiteExecutionDelegate mSQLiteExecutionDelegate;

        /**
         * @param concernedDbPath the path of the database which will be observed and checked
         * @param executionDelegate SQLiteLint will do some sql execution use SQLite, but SQLiteLint will delegate the execution.
         *                          A simple delegate like {@link SimpleSQLiteExecutionDelegate}
         */
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

    /**
     * The first step of SQLiteLint is to collect the executed sqls.
     * Two way to achieve this.
     *
     * #HOOK: We will hook sqlite3_profile, and it will callback to our method when sqls are executed.
     * @see com.tencent.sqlitelint.util.SQLite3ProfileHooker
     * But hook will fail when tha android version is over Androd N.
     * TODO try to break through this limitation
     *
     * #CUSTOM_NOTIFY: It means that the user is responbisable to notify the sql execution.
     * @see #notifySqlExecution(String, String, int)
     * TODO link example
     */
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
