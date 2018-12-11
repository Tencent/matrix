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
import android.database.sqlite.SQLiteDatabase;

import com.tencent.sqlitelint.util.SLog;

/**
 * When using the android-framework-provided {@link SQLiteDatabase} the manage the SQLite database.
 *
 * We can take this simple implement as the delegate for SQLiteLint's sql execution.
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/14.
 */

public final class SimpleSQLiteExecutionDelegate implements ISQLiteExecutionDelegate {
    private static final String TAG = "SQLiteLint.SimpleSQLiteExecutionDelegate";
    private final SQLiteDatabase mDb;

    public SimpleSQLiteExecutionDelegate(SQLiteDatabase db) {
        assert  db != null;

        mDb = db;
    }

    @Override
    public Cursor rawQuery(String selection, String... selectionArgs) throws SQLException {
        if (!mDb.isOpen()) {
            SLog.w(TAG, "rawQuery db close");
            return null;
        }

        return mDb.rawQuery(selection, selectionArgs);
    }

    @Override
    public void execSQL(String sql) throws SQLException {
        if (!mDb.isOpen()) {
            SLog.w(TAG, "rawQuery db close");
            return;
        }

        mDb.execSQL(sql);
    }
}
