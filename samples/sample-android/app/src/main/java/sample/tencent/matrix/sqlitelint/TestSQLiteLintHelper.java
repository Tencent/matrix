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
import android.content.res.AssetManager;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.sqlitelint.SQLiteLintIssue;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.StringWriter;
import java.util.Map;
import java.util.Random;

public class TestSQLiteLintHelper {
    private final static String TAG = "TestSQLiteLintHelper";

    public static String[] getTestSqlList() {
        String[] list = new String[]{
                "select * from testTable",//select *
                "select name from testTable where age>10",//no index
                "select name from testTableRedundantIndex where age&2 != 0",//not use index
                "select name from testTableRedundantIndex where name like 'j%'",//not use index
                "select name from testTableRedundantIndex where name = 'jack' and age > 20",
                "select testTable.name from testTable, testTableAutoIncrement where testTableAutoIncrement.age=testTable.age",
                "select Id from testTable where age = 10 union select Id from testTableRedundantIndex where age > 10",//union
                "select name from testTable order by age",//use tmp tree
                "select name from testTableRedundantIndex where gender=1 and age=5",//bigger index
                "select name, case when age>=18 then 'Adult' else 'child' end LifeStage from testTableRedundantIndex where age > 20 order by age,name,gender",
                "select name,age,gender from testTableRedundantIndex where age > 10 and age < 20 or id between 30 and 40 or id = 1000 ORDER BY name,age,gender desc limit 10 offset 2;",
                "select * from (select * from testTable where age = 18 order by age limit 10) as tb where age = 18 " +
                        "UNION select m.* from testTable AS m, testTableRedundantIndex AS c where m.age = c.age;",
                "SELECT name FROM testTable WHERE name not LIKE 'rt%' OR name LIKE 'rc%' AND age > 20 GROUP BY name ORDER BY age;",
                "SELECT id AS id_alias FROM testTable AS test_alias WHERE id_alias = 1 or id = 2",
                "SELECT name FROM testTable WHERE id = (SELECT id FROM testTableRedundantIndex WHERE name = 'hello world')",
                "SELECT * FROM testTable where name = 'rc' UNION SELECT * FROM testTableWithoutRowid UNION SELECT * FROM testTableAutoIncrement",
                "SELECT name FROM testTable WHERE AGE GLOB '2*';",
                "SELECT DISTINCT name FROM testTable GROUP BY name HAVING count(name) < 2;",
                "SELECT name FROM contact WHERE status = 2;",
                "select rowid from contact where name = 'rr' or age > 12",
                "select t1.name ,t2.age from testTable as t1,testTableRedundantIndex as t2 where t1.id = t2.id and (t1.age=23 or t2.age=12);",
                "select t1.name ,t2.age from testTable as t1,testTableRedundantIndex as t2 where t1.id = t2.id and (t1.age=23 and t2.age=12);",
                "select name,age from contact where name like 'w%' and age > 12",
                "select name,age from contact where name >= 'rc' and age&2=1",
                "select name,age from contact where name = 'r' or age > 12  or status = 1",
        };
        return list;
    }

    /**
     * kExplainQueryScanTable = 1,
     * kExplainQueryUseTempTree = 2,
     * kExplainQueryTipsForLargerIndex = 3,
     * kAvoidAutoIncrement = 4,
     * kAvoidSelectAllChecker = 5,
     * kWithoutRowIdBetter = 6,
     * kPreparedStatementBetter = 7,
     * kRedundantIndex = 8,
     *
     * @param issuesMap
     */
    public static void initIssueList(Context context, Map<String, SQLiteLintIssue> issuesMap) {
        String issuesString = getAssetAsString(context, "sqlite_lint_issues.json");
        try {
            JSONArray jsonArray = new JSONArray(issuesString);
            for (int i = 0; i < jsonArray.length(); i++) {
                JSONObject jsonObject = jsonArray.getJSONObject(i);
                SQLiteLintIssue issue = new SQLiteLintIssue();
                issue.id = jsonObject.getString("id");
                issue.sql = jsonObject.getString("sql");
                issue.type = jsonObject.getInt("type");
                issuesMap.put(issue.id, issue);
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public static String[] getTestParserList() {
        String[] list = new String[]{
                "select a,b,c from test  where id > 100 and id < 200 or id between 300 and 400 or id = 1000 ORDER BY c1,c2,c3 desc limit 10 offset 2;",
                "select t1.a ,t2.b from table1 as t1,table2 as t2 where t1.tid = t2.tid and t1.tid='aaa' or t2.tid=12;",
                "select count(*) from test union select id from test where id not IN (select id from test1);",
                "select title, year from film where starring like 'Jodie%' and year >= 1985 order by year desc limit 10;",
                "SELECT * FROM table1 WHERE column2 not LIKE 'rt' OR column3 LIKE 'rc' AND column1 = 'yy' GROUP BY column2 ORDER BY column2;",
                "SELECT * FROM nosharding_test WHERE id = 1 AND(id < 50 OR id > 200);",
                "SELECT * FROM test WHERE name NOT LIKE '/Hello%' ESCAPE '/';",
                "SELECT id, name FROM testdb.test WHERE id = 100",
                "SELECT DATABASE();",
                "SELECT SLEEP(5)",
                "SELECT * FROM test WHERE id IN (SELECT id FROM test1 WHERE name = 'test')",
                "SELECT id AS id_alias FROM test AS test_alias WHERE id_alias = 1 AND id = 2",
                "SELECT test.id, test1.name FROM test JOIN test1 ON (test.id = test1.id)",
                "SELECT test.id, test1.name FROM test JOIN test1 USING (id,name)",
                "SELECT * FROM test WHERE id = (SELECT id FROM test1 WHERE name = 'hello world')",
                "SELECT test.id, test1.name FROM test full OUTER JOIN test1 ON (test.id = test1.id)",
                "SELECT * FROM test UNION SELECT * FROM test1 UNION SELECT * FROM test2",
                "SELECT name, AVG(price) FROM sales GROUP BY name",
                "SELECT * FROM departments WHERE not EXISTS(SELECT * FROM employees WHERE departments.department_id = employees.department_id);",
                "SELECT * FROM COMPANY GROUP BY name HAVING count(name) < 2;",
                "SELECT DISTINCT name FROM COMPANY;",
                "SELECT * FROM COMPANY WHERE AGE  GLOB '2*';",
                "INSERT INTO testdb.test(id, name) SELECT id, name FROM test1",
                "INSERT INTO testdb.test(id, name) VALUES(1, 'test'), (2, 'test'), (3, 'test');",
                "INSERT INTO testdb.test(id, name) SET id = 1, name = 'test'",
                "INSERT or replace INTO TestInsertOrReplace(id, name) SELECT id, name FROM test1",
                "INSERT or replace INTO TestInsertOrReplace(id, name) VALUES(1, 'test'), (2, 'test'), (3, 'test');",
                "INSERT or replace INTO TestInsertOrReplace(id, name) SET id = 1, name = 'test'",
                "replace INTO TestReplace(id, name) SELECT id, name FROM test1",
                "replace INTO TestReplace(id, name) VALUES(1, 'test'), (2, 'test'), (3, 'test');",
                "replace INTO TestReplace(id, name) SET id = 1, name = 'test'",
                "DELETE FROM test WHERE id IN (SELECT id FROM test WHERE type = 'test' LIMIT 0, 2);",
                "UPDATE test SET name = 'world', age = 25 WHERE name = 'hello';",
                "UPDATE test SET name = 'world' WHERE name IN ('hello', 'test');",
                "UPDATE test SET age = 1 WHERE id > 100 LIMIT 10,2;",
                "UPDATE test SET age = 1 WHERE id&2=1 or id|3=1;",
                "select * from (select * from test where isMine = 1 order by type limit 10) as tb where isMine = 1 " +
                        "UNION select m.* from test AS m, Contact AS c where m.talker = c.openId;",
                "  INSERT INTO MyTable (PriKey, Description) SELECT * FROM SomeView;",
                "SELECT  customerid, firstname, lastname, CASE country WHEN 'China' THEN 'Dosmetic' ELSE" +
                        " 'Foreign' END CustomerGroup FROM customers ORDER BY LastName, FirstName; "//fail for "else"

        };
        return list;
    }

    public static String genRandomString(int length) {
        String str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        Random random = new Random();
        StringBuffer buf = new StringBuffer();
        for (int i = 0; i < length; i++) {
            int num = random.nextInt(62);
            buf.append(str.charAt(num));
        }
        return buf.toString();
    }

    public static String getAssetAsString(Context context, String file) {
        MatrixLog.i(TAG, "getAssetAsString : " + file);
        AssetManager assets = context.getAssets();
        InputStream in = null;
        try {
            in = assets.open(file);
        } catch (Exception e) {
        }
        return convertStreamToString(in);
    }

    public static String convertStreamToString(InputStream in) {
        if (in == null) {
            return null;
        }
        InputStreamReader reader = new InputStreamReader(in);
        char[] buffer = new char[4096];
        StringWriter writer = new StringWriter();
        int n;
        try {
            while (-1 != (n = reader.read(buffer))) {
                writer.write(buffer, 0, n);
            }
        } catch (Exception e) {
            return "";
        } finally {
            qualityClose(reader);
            qualityClose(in);
        }
        return writer.toString();
    }


    public static void qualityClose(Closeable closeable) {
        try {
            if (closeable != null) {
                closeable.close();
            }
        } catch (IOException e) {
        }
    }
}
