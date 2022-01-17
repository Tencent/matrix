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

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.NotificationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class Examples {
    static final String TAG = "Matrix.test.Examples";

    Context mContext;

    @Test
    public void exampleForCpuLoad() {
        if (TestUtils.isAssembleTest()) {
            return;
        } else {
            mockSetup();
        }

        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin monitor = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (monitor != null) {
                CompositeMonitors compositor = new CompositeMonitors(monitor.core());
                compositor.metric(JiffiesMonitorFeature.JiffiesSnapshot.class);
                compositor.metric(CpuStatFeature.CpuStateSnapshot.class);
                compositor.start();

                doSomething();

                compositor.finish();
                int cpuLoad = compositor.getCpuLoad();
                Assert.assertTrue(cpuLoad > 0);
            }
        }
    }

    @Test
    public void exampleForCpuFreqSampling() {
        if (TestUtils.isAssembleTest()) {
            return;
        } else {
            mockSetup();
        }

        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin monitor = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (monitor != null) {
                CompositeMonitors compositor = new CompositeMonitors(monitor.core());
                compositor.sample(DeviceStatMonitorFeature.CpuFreqSnapshot.class, 10L);
                compositor.start();

                doSomething();

                compositor.finish();
                MonitorFeature.Snapshot.Sampler.Result result = compositor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class);
                Assert.assertNotNull(result);
                Assert.assertTrue(result.sampleAvg > 0);
            }
        }
    }

    @Test
    public void exampleForTemperatureSampling() {
        if (TestUtils.isAssembleTest()) {
            return;
        } else {
            mockSetup();
        }

        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin monitor = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (monitor != null) {
                CompositeMonitors compositor = new CompositeMonitors(monitor.core());
                compositor.sample(DeviceStatMonitorFeature.BatteryTmpSnapshot.class, 10L);
                compositor.start();

                doSomething();

                compositor.finish();
                MonitorFeature.Snapshot.Sampler.Result result = compositor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class);
                Assert.assertNotNull(result);
                Assert.assertTrue(result.sampleAvg > 0);
            }
        }
    }

    private void doSomething() {
        try {
            Thread.sleep(1000L);
        } catch (InterruptedException ignored) {
        }
    }

    @Before
    public void setUp() {
        System.setProperty("org.mockito.android.target", ApplicationProvider.getApplicationContext().getCacheDir().getPath());
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

    private void mockSetup() {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();
    }

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enable(LooperTaskMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enable(AppStatMonitorFeature.class)
                .enable(BlueToothMonitorFeature.class)
                .enable(WifiMonitorFeature.class)
                .enable(LocationMonitorFeature.class)
                .enable(TrafficMonitorFeature.class)
                .enable(NotificationMonitorFeature.class)
                .enable(CpuStatFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(true)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }
}
