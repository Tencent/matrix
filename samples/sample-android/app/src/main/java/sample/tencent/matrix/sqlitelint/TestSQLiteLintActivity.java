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

import android.content.ContentValues;
import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Toast;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.sqlitelint.SQLiteLint;
import com.tencent.sqlitelint.SQLiteLintAndroidCoreManager;
import com.tencent.sqlitelint.SQLiteLintIssue;
import com.tencent.sqlitelint.SQLiteLintPlugin;
import com.tencent.sqlitelint.behaviour.BaseBehaviour;
import com.tencent.sqlitelint.behaviour.persistence.IssueStorage;
import com.tencent.sqlitelint.config.SQLiteLintConfig;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import sample.tencent.matrix.R;
import sample.tencent.matrix.issue.IssueFilter;

public class TestSQLiteLintActivity extends AppCompatActivity {
    private final static String TAG = "Matrix.TestSQLiteLintActivity";
    private static int only1 = 0;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.test_sqlite_lint);

        Plugin plugin = Matrix.with().getPluginByClass(SQLiteLintPlugin.class);
        if (!plugin.isPluginStarted()) {
            MatrixLog.i(TAG, "plugin-sqlite-lint start");
            plugin.start();
        }

        IssueFilter.setCurrentFilter(IssueFilter.ISSUE_SQLITELINT);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        IssueStorage.clearData();

        SQLiteLintPlugin plugin = (SQLiteLintPlugin) Matrix.with().getPluginByClass(SQLiteLintPlugin.class);
        if (plugin == null) {
            return;
        }

        if (plugin.isPluginStarted()) {
            plugin.stop();
        }

        stopTest();
    }

    private void insert() {
        ContentValues contentValues = new ContentValues();
        contentValues.put("Id", System.currentTimeMillis());
        contentValues.put("name", TestSQLiteLintHelper.genRandomString(5));
        contentValues.put("age", System.currentTimeMillis() % 90);
        TestDBHelper.get().getWritableDatabase().insert(TestDBHelper.TABLE_NAME, null, contentValues);
    }

    //test PreparedStatementBetterChecker
    private void batchInsert(final int repeatCnt) {
        String sql;
        for (int i = 0; i < repeatCnt; i++) {
            sql = "insert into testTable(Id, name, age) values (" + System.currentTimeMillis() + i + ", '" + TestSQLiteLintHelper.genRandomString(5) + "'"
                    + ", " + System.currentTimeMillis() % 90 + ")";
            TestDBHelper.get().getWritableDatabase().execSQL(sql);
        }

    }

    private void deleteAll() {
        String sql = "delete from testTable";
        TestDBHelper.get().getWritableDatabase().execSQL(sql);
    }

    public void onClick(View v) {
        if (v.getId() == R.id.create_db) {

            toastStartTest("create_db");
            startDBCreateTest();
        }
        if (v.getId() == R.id.start) {

            toastStartTest("start");
            startTest();
        }
    }


    public void toastStartTest(String val) {
        Toast.makeText(this, "starting sqlite lint -> " + val + " test", Toast.LENGTH_SHORT).show();
    }


    private Map<String, SQLiteLintIssue> issueMap = new HashMap<>();
    private Map<String, SQLiteLintIssue> foundIssueMap = new HashMap<>();
    private JSONArray issueJsonArray = new JSONArray();
    private boolean isCalibrationMode = false;

    private void doTest() {
        String[] list = TestSQLiteLintHelper.getTestSqlList();
        /**
         * if use {@link SQLiteLint.SqlExecutionCallbackMode#CUSTOM_NOTIFY}, need SQLiteLint.notifySqlExecution, for example:
         * long start;
         * int cost;
         * final String dbPath = TestDBHelper.get().getWritableDatabase().getPath();
         * for (String sql : list) {
         *     start = System.currentTimeMillis();
         *     Cursor cursor = TestDBHelper.get().getReadableDatabase().rawQuery(sql, null);
         *     cursor.moveToFirst();
         *     cursor.close();
         *     cost = (int) (System.currentTimeMillis() - start);
         *     SQLiteLint.notifySqlExecution(dbPath, sql, cost);
         * }
         * else if use {@link SQLiteLint.SqlExecutionCallbackMode#HOOK}, no need to care about {@link SQLiteLint#notifySqlExecution(String, String, int)}
         * like following below. 
         */
        for (String sql : list) {
            Cursor cursor = TestDBHelper.get().getReadableDatabase().rawQuery(sql, null);
            cursor.moveToFirst();
            cursor.close();
        }

        deleteAll();
        insert();
        batchInsert(40);
    }

    private BaseBehaviour behaviour = new BaseBehaviour() {

        @Override
        public void onPublish(List<SQLiteLintIssue> list) {
            if (isCalibrationMode) {
                for (SQLiteLintIssue issue : list) {
                    if (foundIssueMap.containsKey(issue.id)) {
                        continue;
                    }
                    foundIssueMap.put(issue.id, issue);
                    MatrixLog.i(TAG, String.format("onPublish issue %s, sql: %s, type: %d, desc: %s", issue.id, issue.sql, issue.type, issue.desc));
                    JSONObject jsonObject = new JSONObject();
                    try {
                        jsonObject.put("id", issue.id);
                        jsonObject.put("sql", issue.sql);
                        jsonObject.put("type", issue.type);
                        issueJsonArray.put(jsonObject);
                    } catch (JSONException e) {
                        e.printStackTrace();
                    }
                }
            } else {
                for (SQLiteLintIssue issue : list) {
                    foundIssueMap.put(issue.id, issue);
                }
            }
        }
    };

    private void startTest() {

        MatrixLog.d(TAG, "start test, please wait");
        SQLiteLintAndroidCoreManager.INSTANCE.addBehavior(behaviour, TestDBHelper.get().getReadableDatabase().getPath());
        TestSQLiteLintHelper.initIssueList(this, issueMap);
        doTest();
    }

    private void stopTest() {
        SQLiteLintAndroidCoreManager.INSTANCE.removeBehavior(behaviour, TestDBHelper.get().getReadableDatabase().getPath());
    }

    private void startDBCreateTest() {
        only1 += 1;
        if (only1 > 1) {
            Toast.makeText(this, "install twice!! ignore.", Toast.LENGTH_LONG).show();
        }

        Toast.makeText(this, "start create db, please wait", Toast.LENGTH_LONG).show();
        SQLiteLintPlugin plugin = (SQLiteLintPlugin) Matrix.with().getPluginByClass(SQLiteLintPlugin.class);
        if (plugin == null) {
            return;
        }

        if (!plugin.isPluginStarted()) {
            plugin.start();
        }

        plugin.addConcernedDB(new SQLiteLintConfig.ConcernDb(TestDBHelper.get().getWritableDatabase())
                //.setWhiteListXml(R.xml.sqlite_lint_whitelist)//disable white list by default
                .enableAllCheckers());
    }
}
