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

package com.tencent.matrix.batterycanary.monitor.feature;

import android.app.Application;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureOverAllTest {
    static final String TAG = "Matrix.test.MonitorFeatureOverAllTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
        if (!BatteryEventDelegate.isInit()) {
            BatteryEventDelegate.init((Application) mContext.getApplicationContext());
        }
    }

    @After
    public void shutDown() {
    }

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enable(AppStatMonitorFeature.class)
                .enable(BlueToothMonitorFeature.class)
                .enable(CpuStatFeature.class)
                .enable(TrafficMonitorFeature.class)
                .enable(WifiMonitorFeature.class)
                .enable(LocationMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(true)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .setCallback(new BatteryMonitorCallback.BatteryPrinter() {
                    @Override
                    public BatteryPrinter attach(BatteryMonitorCore monitorCore) {
                        BatteryPrinter core = super.attach(monitorCore);
                        mCompositeMonitors.sample(DeviceStatMonitorFeature.BatteryTmpSnapshot.class, 100L);
                        mCompositeMonitors.sample(DeviceStatMonitorFeature.CpuFreqSnapshot.class, 100L);
                        mCompositeMonitors.sample(DeviceStatMonitorFeature.ChargeWattageSnapshot.class, 100L);
                        mCompositeMonitors.sample(CpuStatFeature.CpuStateSnapshot.class, 100L);
                        mCompositeMonitors.sample(JiffiesMonitorFeature.UidJiffiesSnapshot.class, 100L);
                        mCompositeMonitors.sample(TrafficMonitorFeature.RadioStatSnapshot.class, 100L);
                        return core;
                    }
                })
                .build();


        return new BatteryMonitorCore(config);
    }

    @Test
    public void testForegroundLoopCheck() throws InterruptedException {
        if (TestUtils.isAssembleTest()) return;

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

        final JiffiesMonitorFeature feature = new JiffiesMonitorFeature();
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                monitor.onForeground(true);
            }
        });
        Thread.sleep(5000000L);
    }
}
