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

package com.tencent.matrix.batterycanary.monitor;

import android.app.Application;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;


@RunWith(AndroidJUnit4.class)
public class MonitorCoreTest {
    static final String TAG = "Matrix.test.MonitorCoreTest";

    Context mContext;
    BatteryMonitorCallback.BatteryPrinter spyCallback;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        spyCallback = new BatteryMonitorCallback.BatteryPrinter() {
            @Override
            public void onTraceBegin() {
                super.onTraceBegin();
            }

            @Override
            public void onTraceEnd() {
                super.onTraceEnd();
            }

            @Override
            public void onJiffies(JiffiesMonitorFeature.JiffiesResult result) {
                super.onJiffies(result);
            }

            @Override
            public StringBuilder getExtInfo() {
                return super.getExtInfo();
            }

            @Override
            public void onTaskTrace(Thread thread, List<LooperTaskMonitorFeature.TaskTraceInfo> sortList) {
                super.onTaskTrace(thread, sortList);
            }

            @Override
            public void onWakeLockTimeout(int warningCount, WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord record) {
                super.onWakeLockTimeout(warningCount, record);
            }
        };
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testInitCore() throws InterruptedException {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(LooperTaskMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
    }

    @Test
    public void testJiffiesFeatures() throws InterruptedException {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                    new Handler(Looper.getMainLooper());
                }
            }
        });
        thread.setPriority(Thread.MAX_PRIORITY);
        thread.setName("test-jiffies-thread");
        thread.start();

        Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .setCallback(spyCallback)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        mockForegroundSwitching(core);
        core.start();
        Thread.sleep(10000L);
    }

    @Test
    public void testJiffiesFeaturesWithForegroundLoopCheck() throws InterruptedException {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                    new Handler(Looper.getMainLooper());
                }
            }
        });
        thread.setPriority(Thread.MAX_PRIORITY);
        thread.setName("test-jiffies-thread");
        thread.start();

        Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                core.onForeground(true);
            }
        });
        Thread.sleep(10000L);
    }

    private void mockForegroundSwitching(final BatteryMonitorCore core) {
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                core.onForeground(false);
            }
        });
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                core.onForeground(true);
            }
        }, 2000L);
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                core.onForeground(false);
            }
        }, 5000L);
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                core.onForeground(true);
            }
        }, 9000L);
    }
}
