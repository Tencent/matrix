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
import android.os.Environment;

import com.tencent.sqlitelint.util.SLog;
import com.tencent.sqlitelint.util.SQLiteLintUtil;

import static android.os.Environment.DIRECTORY_DOWNLOADS;


/**
 * As simple As possible. No waste time coding here.
 * <p>
 * dbs path in like:
 * /sdcard/downloads/SQLiteLint-com.tencent.mp/database/
 *
 * @author liyongjie
 *         Created by liyongjie on 17/1/19.
 * @deprecated temperately
 */

public enum SQLiteLintOwnDatabase {
    INSTANCE;

    private static final String TAG = "SQLiteLint.SQLiteLintOwnDatabase";

    private static final String ROOT_PATH = Environment.getExternalStoragePublicDirectory(DIRECTORY_DOWNLOADS).getAbsolutePath();
    private static final String DATABASE_DIRECTORY = "database";
    private static final String DATABASE_NAME = "own.db";

    /**
     * need to init it, when install
     *
     * @see #setOwnDbDirectory(Context)
     */
    private static String sOwnDbDirectory = "";

    /** version history starts **/
    /**
     * init version:
     * A table CheckResult created
     */
    private static final int VERSION_1 = 1;

    /**
     * version history ends
     **/

    private static final int NEW_VERSION = VERSION_1;

    private volatile SQLiteDatabase mDatabase;
    private boolean mIsInitializing;

    public SQLiteDatabase getDatabase() {
        if (mDatabase == null || !mDatabase.isOpen()) {
            synchronized (this) {
                if (mDatabase == null || !mDatabase.isOpen()) {
                    mDatabase = openOrCreateDatabase();
                }
            }
        }

        return mDatabase;
    }

    public synchronized void closeDatabase() {
        if (mIsInitializing) {
            throw new IllegalStateException("Closed during initialization");
        }

        if (mDatabase != null && mDatabase.isOpen()) {
            mDatabase.close();
            mDatabase = null;
        }
    }

    private void onCreate(SQLiteDatabase db) {
        SLog.i(TAG, "onCreate");

        db.execSQL(IssueStorage.DB_VERSION_1_CREATE_SQL);
        for (int i = 0; i < IssueStorage.DB_VERSION_1_CREATE_INDEX.length; i++) {
            db.execSQL(IssueStorage.DB_VERSION_1_CREATE_INDEX[i]);
        }
    }

    private void onUpgrade(SQLiteDatabase db, int oldVersion) {
        SLog.i(TAG, "onUpgrade oldVersion=%d, newVersion=%d", oldVersion, NEW_VERSION);
    }

    /* only called in getDatabase synchronize block */
    private SQLiteDatabase openOrCreateDatabase() {
        if (mIsInitializing) {
            throw new IllegalStateException("getDatabase called recursively");
        }

        if (SQLiteLintUtil.isNullOrNil(sOwnDbDirectory)) {
            throw new IllegalStateException("OwnDbDirectory not init");
        }

        try {
            mIsInitializing = true;

            String databasePath = String.format("%s/%s", sOwnDbDirectory, DATABASE_NAME);
            SLog.i(TAG, "openOrCreateDatabase path=%s", databasePath);
            SQLiteLintUtil.mkdirs(databasePath);
            SQLiteDatabase db = SQLiteDatabase.openDatabase(databasePath, null, SQLiteDatabase.CREATE_IF_NECESSARY, null);
            final int version = db.getVersion();
            if (version != NEW_VERSION) {
                db.beginTransaction();
                try {
                    if (version == 0) {
                        onCreate(db);
                    } else {
                        if (version != NEW_VERSION) {
                            onUpgrade(db, version);
                        }
                    }
                    db.setVersion(NEW_VERSION);
                    db.setTransactionSuccessful();
                } finally {
                    db.endTransaction();
                }
            }
            return db;
        } finally {
            mIsInitializing = false;
        }
    }

    /**
     * init once
     *
     * @param context
     * @see com.tencent.sqlitelint.SQLiteLintAndroidCore
     */
    public static void setOwnDbDirectory(Context context) {
        if (!SQLiteLintUtil.isNullOrNil(sOwnDbDirectory)) {
            return;
        }

        sOwnDbDirectory = String.format("%s/SQLiteLint-%s/%s/", ROOT_PATH, context.getPackageManager(), DATABASE_DIRECTORY);
    }
}
