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

import android.database.Cursor;
import android.database.SQLException;

import com.tencent.sqlitelint.util.SLog;

import java.util.ArrayList;

/**
 * Communicate with the native lib
 *
 * @author wuzhiwen
 *         Created by wuzhiwen on 17/2/15.
 */
//TODO package protected
public class SQLiteLintNativeBridge {
    private static final String TAG = "SQLiteLint.SQLiteLintNativeBridge";

    public static void loadLibrary() {
        System.loadLibrary("SqliteLint-lib");
        SLog.nativeSetLogger(android.util.Log.VERBOSE);
    }

    SQLiteLintNativeBridge() {
    }

    static native void nativeInstall(String dbPath);
    static native void nativeUninstall(String dbPath);

    //TODO rename nativeNotifySqlExecution and long timeCost
    static native void nativeNotifySqlExecute(String dbPath, String sql, long executeTime, String extInfo);

    private native void execSqlCallback(long execSqlCallbackPtr, long paraPtr,String dbName, int nColumn,
                                        String[] columnValue, String[] columnName); //call when sqliteLintExecSql success

    public static native void nativeAddToWhiteList(String dbPath, String[] checkerArr, String[][] whiteListArr);

    public static native void nativeEnableCheckers(String dbPath, String[] enableCheckerArr);


    /**
     * Called from jni when checker published some SQLite issues
     */
    private static void onPublishIssue(final String dbName, final ArrayList<SQLiteLintIssue> publishedIssues) {
        try {
            SQLiteLintAndroidCoreManager.INSTANCE.get(dbName).onPublish(publishedIssues);
        }catch (Throwable e) {
            SLog.e(TAG, "onPublishIssue ex ", e.getMessage());
        }
    }

    //TODO move somewhere else
    //call from jni
    private String[] sqliteLintExecSql(String dbPath, String sql, boolean needCallBack, long execSqlCallbackPtr,long paraPtr) {
        String[] retObj = new String[2];
        try {
            SLog.i(TAG, "dbPath %s, sql is %s ,needCallBack: %b", dbPath, sql, needCallBack);

            retObj[0] = "";
            retObj[1] = "-1"; //0 is SQLITE_OK(define in sqlite3.h), use -1 for others in Android

            ISQLiteExecutionDelegate executionDelegate = null;
            SQLiteLintAndroidCore core = SQLiteLintAndroidCoreManager.INSTANCE.get(dbPath);
            if (core != null) {
                executionDelegate = core.getSQLiteExecutionDelegate();
            }

            if (executionDelegate == null) {
                SLog.w(TAG, "sqliteLintExecSql mExecSqlImp is null");
                return retObj;
            }
            if (needCallBack) {
                try {
                    Cursor cu = executionDelegate.rawQuery(sql);
                    if (cu == null || cu.getCount() < 0) {
                        SLog.w(TAG, "sqliteLintExecSql cu is null");
                        retObj[0] = "Cursor is null";
                    } else {
                        doExecSqlCallback(execSqlCallbackPtr,paraPtr, dbPath, cu);
                        retObj[1] = "0";
                    }
                    if (cu != null) {
                        cu.close();
                    }
                } catch (Exception e) {
                    SLog.w(TAG, "sqliteLintExecSql rawQuery exp: %s", e.getMessage());
                    retObj[0] = e.getMessage();
                }
            } else {
                try {
                    executionDelegate.execSQL(sql);
                    retObj[1] = "0";
                } catch (SQLException e) {
                    SLog.w(TAG, "sqliteLintExecSql execSQL exp: %s", e.getMessage());
                    retObj[0] = e.getMessage();
                }
            }
        } catch (Throwable e) {
            SLog.e(TAG, "sqliteLintExecSql ex ", e.getMessage());
        }
        return retObj;
    }

    private void doExecSqlCallback(long execSqlCallbackPtr,long paraPtr, String dbName, Cursor cu) {
        if (cu == null) {
            SLog.w(TAG, "doExecSqlCallback cu is null");
            return;
        }
        while (cu.moveToNext()) {
            int columnCount = cu.getColumnCount();
            String[] name = new String[columnCount];
            String[] value = new String[columnCount];
            for (int i = 0; i < columnCount; i++) {
                name[i] = cu.getColumnName(i);
                switch (cu.getType(i)) {
                    case Cursor.FIELD_TYPE_BLOB:
                        value[i] = String.valueOf(cu.getBlob(i));
                        break;
                    case Cursor.FIELD_TYPE_INTEGER:
                        value[i] = String.valueOf(cu.getLong(i));
                        break;
                    case Cursor.FIELD_TYPE_STRING:
                        value[i] = String.valueOf(cu.getString(i));
                        break;
                    case Cursor.FIELD_TYPE_FLOAT:
                        value[i] = String.valueOf(cu.getFloat(i));
                        break;
                    default:
                        value[i] = "";
                        break;
                }
            }
            execSqlCallback(execSqlCallbackPtr, paraPtr, dbName, columnCount, value, name);
        }
    }
}
