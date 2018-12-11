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
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.Toast;

import com.tencent.sqlitelint.SQLiteLint;
import com.tencent.sqlitelint.SQLiteLintAndroidCoreManager;
import com.tencent.sqlitelint.SQLiteLintIssue;
import com.tencent.sqlitelint.behaviour.BaseBehaviour;

import junit.framework.Assert;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import sample.tencent.matrix.R;

public class TestSQLiteLintActivity extends AppCompatActivity {
    private final static String TAG = "TestSQLiteLintActivity";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.test_sqlite_lint);
    }

    private void insert(){
        ContentValues contentValues = new ContentValues();
        contentValues.put("Id", System.currentTimeMillis());
        contentValues.put("name", TestSQLiteLintHelper.genRandomString(5));
        contentValues.put("age", System.currentTimeMillis()%90);
        TestDBHelper.get().getWritableDatabase().insert(TestDBHelper.TABLE_NAME, null, contentValues);
    }

    //test PreparedStatementBetterChecker
    private void batchInsert(final int repeatCnt){
        String sql;
        for (int i=0;i<repeatCnt;i++) {
            sql = "insert into testTable(Id, name, age) values (" + System.currentTimeMillis() + ", '" + TestSQLiteLintHelper.genRandomString(5) + "'"
                    + ", " + System.currentTimeMillis()%90 + ")";
            TestDBHelper.get().getWritableDatabase().execSQL(sql);
        }

    }

    private void deleteAll(){
        String sql = "delete from testTable";
        TestDBHelper.get().getWritableDatabase().execSQL(sql);
    }

    public void onClick(View v){
        if (v.getId() == R.id.start) {
            startTest();
        } else if (v.getId() == R.id.log_result) {
            logTestResult();
        } else if (v.getId() == R.id.stop) {
            stopTest();
        } else if (v.getId() == R.id.test_parse){
            testParser();
        }
    }

    private Map<String,SQLiteLintIssue> issueMap = new HashMap<>();
    private Map<String,SQLiteLintIssue> foundIssueMap = new HashMap<>();
    private JSONArray issueJsonArray = new JSONArray();
    private boolean isCalibrationMode = false;
    private BaseBehaviour behaviour = new BaseBehaviour() {
        @Override
        public void onPublish(List<SQLiteLintIssue> list) {
            if (isCalibrationMode) {
                for (SQLiteLintIssue issue : list) {
                    if (foundIssueMap.containsKey(issue.id)) {
                        continue;
                    }
                    foundIssueMap.put(issue.id, issue);
                    Log.i(TAG, String.format("onPublish issue %s, sql: %s, type: %d, desc: %s", issue.id, issue.sql, issue.type, issue.desc));
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

    private void doTest() {
        String[] list = TestSQLiteLintHelper.getTestSqlList();

        for (String sql:list) {
            Cursor cursor = TestDBHelper.get().getReadableDatabase().rawQuery(sql,null);
            cursor.moveToFirst();
            cursor.close();
            Log.d(TAG,sql + "\tQueryPlan:\n" + getQueryPlan(sql));
        }
        deleteAll();
        insert();
        batchInsert(40);
    }

    private void startTest(){
        SQLiteLintAndroidCoreManager.INSTANCE.addBehavior(behaviour, TestDBHelper.get().getReadableDatabase().getPath());
        TestSQLiteLintHelper.initIssueList(this,issueMap);
        doTest();
    }

    private void stopTest(){
        SQLiteLintAndroidCoreManager.INSTANCE.removeBehavior(behaviour, TestDBHelper.get().getReadableDatabase().getPath());
    }

    /**
     * Do after "start test", and don't do anything else before.
     * Note: Lint::InitCheck will delay 4 sedconds to execute.
     */
    private void logTestResult(){
        if (isCalibrationMode) {
            Log.i(TAG,"printResult: " + issueJsonArray.toString());
        }else {
            Log.i(TAG,String.format("printResult: issueMap.size %d, foundIssueMap.size %d",issueMap.size(), foundIssueMap.size()));
            boolean success = true;
            for (SQLiteLintIssue issue : issueMap.values()){
                if (!foundIssueMap.containsKey(issue.id)) {
                    success = false;
                    Log.i(TAG, String.format("printResult:not found issue %s, sql: %s, type: %d, desc: %s, QueryPlan:\n %s", issue.id,
                            issue.sql, issue.type, issue.desc, getQueryPlan(issue.sql)));
                }
            }
            for (SQLiteLintIssue issue : foundIssueMap.values()){
                if (!issueMap.containsKey(issue.id)) {
                    success = false;
                    Log.i(TAG, String.format("printResult:mistakeIssue %s, sql: %s, type: %d, desc: %s, QueryPlan:\n %s", issue.id, issue.sql,
                            issue.type, issue.desc, getQueryPlan(issue.sql)));
                }
            }
            Assert.assertTrue("printResult: not success", success);
            Toast.makeText(this,"Test ok!",Toast.LENGTH_LONG).show();
        }
    }

    private void testParser(){
        SQLiteLintAndroidCoreManager.INSTANCE.removeBehavior(behaviour, TestDBHelper.get().getReadableDatabase().getPath());
        String[] list = TestSQLiteLintHelper.getTestParserList();
        for (String sql:list) {
            Log.i(TAG, "testParser, sql = " + sql);
            SQLiteLint.notifySqlExecution(TestDBHelper.get().getWritableDatabase().getPath(), sql, 10);
        }
    }

    private String getQueryPlan(String sql){
        if (TextUtils.isEmpty(sql)){
            return null;
        }
        String plan = "";
        sql = "explain query plan " + sql;
        Cursor cursor = TestDBHelper.get().getReadableDatabase().rawQuery(sql,null);
        while (cursor.moveToNext()) {
            plan += cursor.getString(3) + '\n';
        }
        cursor.close();
        return plan;
    }
}
