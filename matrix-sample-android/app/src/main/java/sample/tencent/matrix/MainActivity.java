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

package sample.tencent.matrix;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;

import com.tencent.matrix.util.MatrixLog;

import sample.tencent.matrix.battery.TestBatteryActivity;
import sample.tencent.matrix.device.TestDeviceActivity;
import sample.tencent.matrix.io.TestIOActivity;
import sample.tencent.matrix.resource.TestLeakActivity;
import sample.tencent.matrix.sqlitelint.TestSQLiteLintActivity;
import sample.tencent.matrix.trace.TestTraceMainActivity;


public class MainActivity extends AppCompatActivity {
    private static final String TAG = "Matrix.MainActivity";

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "[onResume]");
        for (int i = 0; i < 10; i++) {
            new HandlerThread("test-"+i).start();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button testTrace = (Button) findViewById(R.id.test_trace);
        testTrace.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestTraceMainActivity.class);
                startActivity(intent);
            }
        });

        Button testIO = (Button) findViewById(R.id.test_io);
        testIO.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestIOActivity.class);
                startActivity(intent);
            }
        });

        Button testLeak = (Button) findViewById(R.id.test_leak);
        testLeak.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestLeakActivity.class);
                startActivity(intent);
            }
        });

        Button testSQLiteLint = (Button) findViewById(R.id.test_sqlite_lint);
        testSQLiteLint.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestSQLiteLintActivity.class);
                startActivity(intent);
            }
        });

        Button testBattery = (Button) findViewById(R.id.test_battery);
        testBattery.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestBatteryActivity.class);
                startActivity(intent);
            }
        });

        Button killSelf = (Button) findViewById(R.id.killself);
        killSelf.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                android.os.Process.killProcess(android.os.Process.myPid());

            }
        });

        Button testDevice = (Button) findViewById(R.id.test_device);
        testDevice.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestDeviceActivity.class);
                startActivity(intent);

            }
        });

        final Handler handler = new Handler(Looper.getMainLooper());
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                MatrixLog.i("Matrix.ThreadWatcher", "RUN:%s", Process.myTid());
//                test(new String[100]);
            }
        }, 3000);


    }

    private void test(String[] strs) {
        String[] next = new String[10000];
        int i = 0;
        for (String s : strs) {
            next[i++] = s;
        }
        SystemClock.sleep(10);
        test(next);
    }


}
