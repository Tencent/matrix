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

package sample.tencent.matrix.sqlitelint;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import sample.tencent.matrix.MatrixApplication;

public class TestDBHelper extends SQLiteOpenHelper {
    private static final int DB_VERSION = 1;
    private static final String DB_NAME = "sqliteLintTest.db";
    public static final String TABLE_NAME = "testTable";
    public static final String TABLE_NAME_AUTO_INCREMENT = "testTableAutoIncrement";
    public static final String TABLE_NAME_WITHOUT_ROWID_BETTER = "testTableWithoutRowid";
    public static final String TABLE_NAME_Redundant_index = "testTableRedundantIndex";
    public static final String TABLE_NAME_CONTACT = "contact";
    private static TestDBHelper mHelper = null;

    public static TestDBHelper get() {
        if (mHelper == null) {
            synchronized (TestDBHelper.class) {
                if (mHelper == null) {
                    mHelper = new TestDBHelper(MatrixApplication.getContext());
                }
            }
        }
        return mHelper;
    }

    public TestDBHelper(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase sqLiteDatabase) {
        String sql = "create table if not exists " + TABLE_NAME + " (Id integer primary key, name text, age integer)";
        sqLiteDatabase.execSQL(sql);
        String sqlAutoIncrement = "create table if not exists " + TABLE_NAME_AUTO_INCREMENT + " (Id integer primary key AUTOINCREMENT, name text, age integer)";
        sqLiteDatabase.execSQL(sqlAutoIncrement);
        String sqlWithoutRowId = "create table if not exists " + TABLE_NAME_WITHOUT_ROWID_BETTER + " (Id text primary key, name integer, age integer)";
        sqLiteDatabase.execSQL(sqlWithoutRowId);
        String sqlRedundantIndex = "create table if not exists " + TABLE_NAME_Redundant_index + " (Id text, name text, age integer, gender integer)";
        sqLiteDatabase.execSQL(sqlRedundantIndex);
        String indexSql = "create index if not exists index_age on " + TABLE_NAME_Redundant_index + "(age);";
        String indexSql2 = "create index if not exists index_age_name on " + TABLE_NAME_Redundant_index + "(age, name);";
        String indexSql3 = "create index if not exists index_name_age on " + TABLE_NAME_Redundant_index + "(name,age);";
        String indexSql4 = "create index if not exists index_id on " + TABLE_NAME_Redundant_index + "(Id);";

        sqLiteDatabase.execSQL(indexSql);
        sqLiteDatabase.execSQL(indexSql2);
        sqLiteDatabase.execSQL(indexSql3);
        sqLiteDatabase.execSQL(indexSql4);

        String contact = "create table if not exists " + TABLE_NAME_CONTACT + " (Id integer primary key, name text, age integer, gender integer, status integer)";
        sqLiteDatabase.execSQL(contact);
        String contactIndex = "create index if not exists index_age_name_status on " + TABLE_NAME_CONTACT + "(age, name, status);";
        String contactIndex2 = "create index if not exists index_name_age_status on " + TABLE_NAME_CONTACT + "(name, age, status);";
        String contactStatusIndex = "create index if not exists index_status on " + TABLE_NAME_CONTACT + "(status);";
        sqLiteDatabase.execSQL(contactIndex);
        sqLiteDatabase.execSQL(contactIndex2);
        sqLiteDatabase.execSQL(contactStatusIndex);
    }


    @Override
    public void onUpgrade(SQLiteDatabase sqLiteDatabase, int oldVersion, int newVersion) {
        String sql = "DROP TABLE IF EXISTS " + TABLE_NAME;
        sqLiteDatabase.execSQL(sql);
        String sql2 = "DROP TABLE IF EXISTS " + TABLE_NAME_AUTO_INCREMENT;
        sqLiteDatabase.execSQL(sql2);
        String sql3 = "DROP TABLE IF EXISTS " + TABLE_NAME_WITHOUT_ROWID_BETTER;
        sqLiteDatabase.execSQL(sql3);
        onCreate(sqLiteDatabase);
    }
}
