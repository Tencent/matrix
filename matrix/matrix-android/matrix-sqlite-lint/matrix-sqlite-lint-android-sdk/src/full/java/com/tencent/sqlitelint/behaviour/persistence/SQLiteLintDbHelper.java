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

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import com.tencent.sqlitelint.util.SLog;

/**
 * Almost same functionality as {@link SQLiteLintOwnDatabase}, a singleton to get the SQLiteDatabase,
 * but SQLiteLintOwnDatabase can determine the path of the db file
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/22.
 */

public enum SQLiteLintDbHelper {
    INSTANCE;

    private static final String TAG = "SQLiteLint.SQLiteLintOwnDatabase";

    private static final String DB_NAME = "SQLiteLintInternal.db";
    private static final int VERSION_1 = 1;

    private volatile InternalDbHelper mHelper;

    public SQLiteDatabase getDatabase() {
        if (mHelper == null) {
            throw new IllegalStateException("getIssueStorage db not ready");
        }

        return mHelper.getWritableDatabase();
    }

    /**
     * initialize in {@link com.tencent.sqlitelint.SQLiteLintAndroidCore}
     * ensures initialized before all the SQLiteLint own db operations begins
     *
     * @param context
     */
    public void initialize(Context context) {
        if (mHelper == null) {
            synchronized (this) {
                if (mHelper == null) {
                    mHelper = new InternalDbHelper(context);
                }
            }
        }
    }

    private static final class InternalDbHelper extends SQLiteOpenHelper {
        private static final String DB_NAME = "SQLiteLintInternal.db";
        private static final int VERSION_1 = 1;

        InternalDbHelper(Context context) {
            super(context, DB_NAME, null, VERSION_1);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            SLog.i(TAG, "onCreate");

            db.execSQL(IssueStorage.DB_VERSION_1_CREATE_SQL);
            for (int i = 0; i < IssueStorage.DB_VERSION_1_CREATE_INDEX.length; i++) {
                db.execSQL(IssueStorage.DB_VERSION_1_CREATE_INDEX[i]);
            }
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        }
    }
}
