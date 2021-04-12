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

package com.tencent.matrix.batterycanary;

import android.app.Application;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature.AlarmSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature.BlueToothSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature.LocationSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature.WifiSnapshot;
import com.tencent.matrix.plugin.Plugin;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.Callable;

import static org.junit.Assert.*;

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(AndroidJUnit4.class)
public class ApisTest {
    static final String TAG = "Matrix.test.foo";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testMonitorPluginConfigs() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                // Thread Activities Monitor
                .enable(JiffiesMonitorFeature.class)
                .enableStatPidProc(true)
                .greyJiffiesTime(30 * 1000L)
                .enableBackgroundMode(false)
                .backgroundLoopCheckTime(30 * 60 * 1000L)
                .enableForegroundMode(true)
                .foregroundLoopCheckTime(20 * 60 * 1000L)
                .setBgThreadWatchingLimit(5000)
                .setBgThreadWatchingLimit(8000)

                // App & Device Status Monitor For Better Invalid Battery Activities Configure
                .setOverHeatCount(1024)
                .enable(DeviceStatMonitorFeature.class)
                .enable(AppStatMonitorFeature.class)
                .setSceneSupplier(new Callable<String>() {
                    @Override
                    public String call() {
                        return "Current AppScene";
                    }
                })

                // AMS Activities Monitor:
                // alarm/wakelock watch
                .enableAmsHook(true)
                .enable(AlarmMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .wakelockTimeout(2 * 60 * 1000L)
                .wakelockWarnCount(3)
                .addWakeLockWhiteList("Ignore WakeLock TAG1")
                .addWakeLockWhiteList("Ignore WakeLock TAG2")
                // scanning watch (wifi/gps/bluetooth)
                .enable(WifiMonitorFeature.class)
                .enable(LocationMonitorFeature.class)
                .enable(BlueToothMonitorFeature.class)

                // Lab Feature:
                // network monitor
                // looper task monitor
                .enable(TrafficMonitorFeature.class)
                .enable(LooperTaskMonitorFeature.class)
                .addLooperBlackList("HandlerThread Name To Watch")
                .useThreadClock(false)
                .enableAggressive(false)

                // Monitor Callback
                .setCallback(new BatteryMonitorCallback.BatteryPrinter() {
                    @Override
                    public void onWakeLockTimeout(WakeLockRecord record, long backgroundMillis) {
                        // WakeLock acquired too long
                    }
                    @Override
                    protected void onCanaryDump(AppStats appStats) {
                        // Dump battery stats data periodically
                        long statMinute = appStats.getMinute();
                        boolean foreground = appStats.isForeground();
                        boolean charging = appStats.isCharging();
                        super.onCanaryDump(appStats);
                    }
                    @Override
                    protected void onReportJiffies(@NonNull Delta<JiffiesSnapshot> delta) {
                        // Report all threads jiffies consumed during the statMinute time
                    }
                    @Override
                    protected void onReportAlarm(@NonNull Delta<AlarmSnapshot> delta) {
                        // Report all alarm set during the statMinute time
                    }
                    @Override
                    protected void onReportWakeLock(@NonNull Delta<WakeLockSnapshot> delta) {
                        // Report all wakelock acquired during the statMinute time
                    }
                    @Override
                    protected void onReportBlueTooth(@NonNull Delta<BlueToothSnapshot> delta) {
                        // Report all bluetooth scanned during the statMinute time
                    }
                    @Override
                    protected void onReportWifi(@NonNull Delta<WifiSnapshot> delta) {
                        // Report all wifi scanned during the statMinute time
                    }
                    @Override
                    protected void onReportLocation(@NonNull Delta<LocationSnapshot> delta) {
                        // Report all gps scanned during the statMinute time
                    }
                })
                .build();

        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);

        Assert.assertEquals("BatteryMonitorPlugin", plugin.getTag());
    }

    /**
     * Run this test case individually and checkout logcat output with tag 'PowerTest'.
     */
    @Test
    public void testForegroundLoopCheckAsShowCase() throws InterruptedException {
        if (TestUtils.isAssembleTest()) return;
        BatteryEventDelegate.init((Application) mContext.getApplicationContext());

        // Demo battery-used thread
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


        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enableStatPidProc(true)
                .greyJiffiesTime(100)
                .enableForegroundMode(true)
                .foregroundLoopCheckTime(1000)

                .enable(DeviceStatMonitorFeature.class)
                .enable(AppStatMonitorFeature.class)
                .setSceneSupplier(new Callable<String>() {
                    @Override
                    public String call() {
                        return "UnitTest";
                    }
                })

                .enableAmsHook(true)
                .enable(AlarmMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(WifiMonitorFeature.class)
                .enable(LocationMonitorFeature.class)
                .enable(BlueToothMonitorFeature.class)

                .enable(TrafficMonitorFeature.class)
                .enable(LooperTaskMonitorFeature.class)
                .useThreadClock(false)
                .enableAggressive(true)

                .setCallback(new BatteryMonitorCallback.BatteryPrinter())
                .build();

        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        Matrix.init(
                new Matrix.Builder(((Application) mContext.getApplicationContext()))
                        .plugin(plugin)
                        .build()
        );

        final BatteryMonitorCore monitor = plugin.core();
        monitor.enableForegroundLoopCheck(true);
        monitor.start();
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                monitor.onForeground(true);
            }
        });

        Thread.sleep(Integer.MAX_VALUE);
    }
}
