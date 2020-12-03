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
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
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
            public void onTraceEnd(boolean isForeground) {
                super.onTraceEnd(isForeground);
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
                .enable(LooperTaskMonitorFeature.class)
                .enable(JiffiesMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(true)
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
    public void testInitCoreWithCustomCfg0() throws InterruptedException {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(LooperTaskMonitorFeature.class)
                .enable(JiffiesMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(true)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(300)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(2000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        Thread.sleep(1000L);
    }

    @Test
    public void testInitCoreWithCustomCfg1() throws InterruptedException {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                // .enable(LooperTaskMonitorFeature.class)
                // .enable(JiffiesMonitorFeature.class)
                // .enable(DeviceStatMonitorFeature.class)
                // .enable(WakeLockMonitorFeature.class)
                // .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(true)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(300)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(2000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        Thread.sleep(1000L);
    }

    @Test
    public void testInitCoreWithCustomCfg3() throws InterruptedException {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                // .enable(LooperTaskMonitorFeature.class)
                .enable(JiffiesMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(true)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(300)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(2000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        Thread.sleep(1000L);
    }
    @Test
    public void testInitCoreWithCustomCfg4() throws InterruptedException {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(LooperTaskMonitorFeature.class)
                // .enable(JiffiesMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(true)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(300)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(2000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        Thread.sleep(1000L);
    }
    @Test
    public void testInitCoreWithCustomCfg5() throws InterruptedException {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(LooperTaskMonitorFeature.class)
                .enable(JiffiesMonitorFeature.class)
                // .enable(DeviceStatMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(true)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(300)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(2000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        Thread.sleep(1000L);
    }
    @Test
    public void testInitCoreWithCustomCfg6() throws InterruptedException {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(LooperTaskMonitorFeature.class)
                .enable(JiffiesMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                // .enable(WakeLockMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(true)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(300)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(2000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        Thread.sleep(1000L);
    }
    @Test
    public void testInitCoreWithCustomCfg7() throws InterruptedException {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(LooperTaskMonitorFeature.class)
                .enable(JiffiesMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                // .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(true)
                .enableForegroundMode(true)
                .setCallback(spyCallback)
                .wakelockTimeout(300)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(2000)
                .build();

        final BatteryMonitorCore core = new BatteryMonitorCore(config);
        core.start();
        Thread.sleep(1000L);
    }
}
