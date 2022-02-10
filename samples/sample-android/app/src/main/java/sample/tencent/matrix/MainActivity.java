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

import android.app.Service;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.tencent.matrix.lifecycle.owners.OverlayWindowLifecycleOwner;
import com.tencent.matrix.memory.canary.MemInfo;
import com.tencent.matrix.trace.view.FrameDecorator;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.ViewDumper;

import sample.tencent.matrix.battery.TestBatteryActivity;
import sample.tencent.matrix.hooks.TestHooksActivity;
import sample.tencent.matrix.io.TestIOActivity;
import sample.tencent.matrix.issue.IssuesMap;
import sample.tencent.matrix.kt.lifecycle.TestFgService;
import sample.tencent.matrix.memory.MemInfoTest;
import sample.tencent.matrix.resource.TestLeakActivity;
import sample.tencent.matrix.sqlitelint.TestSQLiteLintActivity;
import sample.tencent.matrix.trace.TestTraceMainActivity;
import sample.tencent.matrix.traffic.TestTrafficActivity;


public class MainActivity extends AppCompatActivity {
    private static final String TAG = "Matrix.sample.MainActivity";

    @Override
    protected void onResume() {
        super.onResume();
        IssuesMap.clear();

        MatrixLog.d(TAG, "has visible window %s", OverlayWindowLifecycleOwner.INSTANCE.hasVisibleWindow());

        new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
            @Override
            public void run() {
                MatrixLog.d(TAG, "has visible window %s", OverlayWindowLifecycleOwner.INSTANCE.hasVisibleWindow());

                String[] arr = ViewDumper.dump();
                MatrixLog.d(TAG, "view tree size = %s", arr.length);
                for (String s : arr) {
                    MatrixLog.d(TAG, "%s\n", s);
                }

            }
        }, 3000);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button testTrace = findViewById(R.id.test_trace);
        testTrace.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestTraceMainActivity.class);
                startActivity(intent);
            }
        });

        Button testIO = findViewById(R.id.test_io);
        testIO.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestIOActivity.class);
                startActivity(intent);
            }
        });

        Button testLeak = findViewById(R.id.test_leak);
        testLeak.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestLeakActivity.class);
                startActivity(intent);
            }
        });

        Button testSQLiteLint = findViewById(R.id.test_sqlite_lint);
        testSQLiteLint.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestSQLiteLintActivity.class);
                startActivity(intent);
            }
        });

        Button testBattery = findViewById(R.id.test_battery);
        testBattery.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestBatteryActivity.class);
                startActivity(intent);
            }
        });

        Button testHooks = findViewById(R.id.test_hooks);
        testHooks.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestHooksActivity.class);
                startActivity(intent);
            }
        });

        Button testTraffic = findViewById(R.id.test_traffic_enter);
        testTraffic.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, TestTrafficActivity.class);
                startActivity(intent);
            }
        });

        long begin = System.currentTimeMillis();
        MemInfo info = MemInfo.getCurrentProcessFullMemInfo();
        MatrixLog.d(TAG, "mem cost %s", System.currentTimeMillis() - begin);
        MatrixLog.d(TAG, "%s", info);

        new Thread(new Runnable() {
            @Override
            public void run() {
                MemInfoTest.test();
            }
        }).start();

        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                        WindowManager.LayoutParams.TYPE_APPLICATION_ATTACHED_DIALOG,
                        WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
                                WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE |
                                WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM,
                        PixelFormat.RGBA_8888);
                getWindowManager().addView(new LinearLayout(MainActivity.this), params);
            }
        }, 500);
    }

    public static class StubService extends Service {
        @Nullable
        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }
    }

    public void testSupervisor(View view) {
        Intent intent = new Intent(this, StubService.class);
        startService(intent);
    }

    public void testFgService(View view) {
        TestFgService.test(this);
    }

    public void testOverlayWindow(View view) {
        if (FrameDecorator.getInstance(getApplicationContext()).isShowing()) {
            FrameDecorator.getInstance(getApplicationContext()).dismiss();
        } else {
            FrameDecorator.getInstance(getApplicationContext()).setEnable(true);
            FrameDecorator.getInstance(getApplicationContext()).show();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
//        new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
//            @Override
//            public void run() {
//                MatrixLog.d(TAG, "finishAndRemoveTask");
//                ActivityManager am = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
//                for (ActivityManager.AppTask appTask : am.getAppTasks()) {
//                    appTask.finishAndRemoveTask();
//                }
//            }
//        }, 5 * 1000);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        MatrixLog.d(TAG, "onDestroy");
    }
}
