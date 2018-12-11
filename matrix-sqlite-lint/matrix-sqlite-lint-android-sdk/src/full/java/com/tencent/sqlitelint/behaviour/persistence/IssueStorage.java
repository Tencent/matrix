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

import android.database.Cursor;
import android.database.sqlite.SQLiteStatement;

import com.tencent.sqlitelint.SQLiteLintIssue;
import com.tencent.sqlitelint.util.SLog;
import com.tencent.sqlitelint.util.SQLiteLintUtil;

import java.util.ArrayList;
import java.util.List;

/**
 * Table to store issues
 * Offers persistence infos of issues to the behaviours
 *
 * @author liyongjie
 *         Created by liyongjie on 17/1/19.
 * @see
 */

public class IssueStorage {
    private static final String TAG = "SQLiteLint.IssueStorage";

    public static final String TABLE_NAME = "Issue";
    public static final String COLUMN_ID = "id";
    public static final String COLUMN_DB_PATH = "dbPath";
    public static final String COLUMN_LEVEL = "level";
    public static final String COLUMN_DESC = "desc";
    public static final String COLUMN_DETAIL = "detail";
    public static final String COLUMN_ADVICE = "advice";
    public static final String COLUMN_CREATE_TIME = "createTime";
    public static final String COLUMN_EXT_INFO = "extInfo";
    public static final String COLUMN_SQL_TIME_COST = "sqlTimeCost";


    public static final String DB_VERSION_1_CREATE_SQL = String.format("CREATE TABLE IF NOT EXISTS %s "
                    + "(%s TEXT PRIMARY KEY NOT NULL, %s TEXT NOT NULL, %s INTEGER, %s TEXT, %s TEXT, %s TEXT, %s INTEGER, %s TEXT, %s INTEGER)", TABLE_NAME,
            COLUMN_ID, COLUMN_DB_PATH, COLUMN_LEVEL, COLUMN_DESC, COLUMN_DETAIL, COLUMN_ADVICE, COLUMN_CREATE_TIME, COLUMN_EXT_INFO, COLUMN_SQL_TIME_COST);
    public static final String[] DB_VERSION_1_CREATE_INDEX = {
            String.format("CREATE INDEX IF NOT EXISTS %s ON %s(%s)", "DbLabel_Index", TABLE_NAME, COLUMN_DB_PATH),
            String.format("CREATE INDEX IF NOT EXISTS %s ON %s(%s,%s)", "DbLabel_CreateTime_Index", TABLE_NAME, COLUMN_DB_PATH, COLUMN_CREATE_TIME),
    };

    private static SQLiteStatement sInsertAllSqlStatement;

    public static boolean saveIssue(SQLiteLintIssue issue) {
        if (hasIssue(issue.id)) {
            SLog.i(TAG, "saveIssue already recorded id=%s", issue.id);
            return false;
        }

        return doInsertIssue(issue);
    }

    public static void saveIssues(List<SQLiteLintIssue> issues) {
        SQLiteLintDbHelper.INSTANCE.getDatabase().beginTransaction();
        try {
            for (int i = 0; i < issues.size(); i++) {
                saveIssue(issues.get(i));
            }

            SQLiteLintDbHelper.INSTANCE.getDatabase().setTransactionSuccessful();
        } finally {
            SQLiteLintDbHelper.INSTANCE.getDatabase().endTransaction();

        }
    }

    public static boolean hasIssue(String id) {
        String querySql = String.format("SELECT %s FROM %s WHERE %s='%s' limit 1", COLUMN_ID, TABLE_NAME, COLUMN_ID, id);
        Cursor cursor = SQLiteLintDbHelper.INSTANCE.getDatabase().rawQuery(querySql, null);
        try {
            return cursor.getCount() > 0;
        } finally {
            cursor.close();
        }
    }

    private static boolean doInsertIssue(SQLiteLintIssue issue) {
        SQLiteStatement insertSql = getInsertAllSqlStatement();
        insertSql.bindString(1, issue.id);
        insertSql.bindString(2, issue.dbPath);
        insertSql.bindLong(3, issue.level);
        insertSql.bindString(4, SQLiteLintUtil.nullAsNil(issue.desc));
        insertSql.bindString(5, SQLiteLintUtil.nullAsNil(issue.detail));
        insertSql.bindString(6, SQLiteLintUtil.nullAsNil(issue.advice));
        insertSql.bindLong(7, issue.createTime);
        insertSql.bindString(8, issue.extInfo);
        insertSql.bindLong(9, issue.sqlTimeCost);
        long r = insertSql.executeInsert();

        SLog.d(TAG, "saveIssue insert ret=%s, id=%s", r, issue.id);
        if (r == -1) {
            SLog.e(TAG, "addIssue failed");
            return false;
        }

        return true;
    }


    public static List<SQLiteLintIssue> getIssueListByDb(String dbLabel) {
        List<SQLiteLintIssue> issueList = new ArrayList<>();
        if (SQLiteLintUtil.isNullOrNil(dbLabel)) {
            return issueList;
        }

        String querySql = String.format("SELECT * FROM %s where %s='%s' ORDER BY %s DESC", TABLE_NAME, COLUMN_DB_PATH, dbLabel, COLUMN_CREATE_TIME);
        Cursor cursor = SQLiteLintDbHelper.INSTANCE.getDatabase().rawQuery(querySql, null);
        try {
            while (cursor.moveToNext()) {
                issueList.add(issueConvertFromCursor(cursor));
            }
        } finally {
            cursor.close();
        }

        return issueList;
    }


    public static List<String> getDbPathList() {
        List<String> dbPathList = new ArrayList<>();
        String querySql = String.format("SELECT DISTINCT(%s) FROM %s", COLUMN_DB_PATH, TABLE_NAME);
        Cursor cursor = SQLiteLintDbHelper.INSTANCE.getDatabase().rawQuery(querySql, null);
        try {
            while (cursor.moveToNext()) {
                dbPathList.add(cursor.getString(cursor.getColumnIndex(COLUMN_DB_PATH)));
            }
        } finally {
            cursor.close();
        }
        return dbPathList;
    }

    /**
     * Mostly new insert rowid is one more than the largest ROWID currently in use
     * @return
     */
    public static long getLastRowId() {
        String querySql = String.format("SELECT rowid FROM %s order by rowid desc limit 1", TABLE_NAME);
        Cursor cursor = SQLiteLintDbHelper.INSTANCE.getDatabase().rawQuery(querySql, null);
        try {
            if (cursor != null && cursor.getCount() > 0) {
                cursor.moveToFirst();
                return cursor.getLong(0);
            }

            return -1;
        } finally {
            cursor.close();
        }
    }

    private static SQLiteStatement getInsertAllSqlStatement() {
        if (sInsertAllSqlStatement == null) {
            sInsertAllSqlStatement = SQLiteLintDbHelper.INSTANCE.getDatabase().compileStatement(String.format("INSERT INTO %s VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)", TABLE_NAME));
        }
        return sInsertAllSqlStatement;
    }

    private static SQLiteLintIssue issueConvertFromCursor(Cursor cursor) {
        SQLiteLintIssue issue = new SQLiteLintIssue();
        issue.id = cursor.getString(cursor.getColumnIndex(COLUMN_ID));
        issue.dbPath = cursor.getString(cursor.getColumnIndex(COLUMN_DB_PATH));
        issue.level = cursor.getInt(cursor.getColumnIndex(COLUMN_LEVEL));
        issue.desc = cursor.getString(cursor.getColumnIndex(COLUMN_DESC));
        issue.detail = cursor.getString(cursor.getColumnIndex(COLUMN_DETAIL));
        issue.advice = cursor.getString(cursor.getColumnIndex(COLUMN_ADVICE));
        issue.createTime = cursor.getLong(cursor.getColumnIndex(COLUMN_CREATE_TIME));
        issue.extInfo = cursor.getString(cursor.getColumnIndex(COLUMN_EXT_INFO));
        issue.sqlTimeCost = cursor.getLong(cursor.getColumnIndex(COLUMN_SQL_TIME_COST));
        return issue;
    }
}
