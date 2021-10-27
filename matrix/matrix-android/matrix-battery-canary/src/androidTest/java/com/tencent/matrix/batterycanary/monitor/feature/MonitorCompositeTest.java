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

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class MonitorCompositeTest {
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

    @Test
    public void testMetric() {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();

        CompositeMonitors compositeMonitor = new CompositeMonitors(monitor);
        Assert.assertNull(compositeMonitor.mBgnSnapshots.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mBgnSnapshots.get(CpuStatFeature.CpuStateSnapshot.class));
        compositeMonitor.metric(JiffiesMonitorFeature.JiffiesSnapshot.class);
        compositeMonitor.configureSnapshots();
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mBgnSnapshots.get(CpuStatFeature.CpuStateSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStatFeature.CpuStateSnapshot.class));
        compositeMonitor.configureDeltas();
        Assert.assertNotNull(compositeMonitor.mDeltas.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStatFeature.CpuStateSnapshot.class));

        compositeMonitor.clear();
        compositeMonitor
                .metric(JiffiesMonitorFeature.JiffiesSnapshot.class)
                .metric(CpuStatFeature.CpuStateSnapshot.class);
        compositeMonitor.configureSnapshots();
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(CpuStatFeature.CpuStateSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStatFeature.CpuStateSnapshot.class));
        compositeMonitor.configureDeltas();
        Assert.assertNotNull(compositeMonitor.mDeltas.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNotNull(compositeMonitor.mDeltas.get(CpuStatFeature.CpuStateSnapshot.class));
    }

    @Test
    public void testMetricAll()  {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();

        CompositeMonitors compositeMonitor = new CompositeMonitors(monitor);
        compositeMonitor.metricAll();

        for (Class<? extends MonitorFeature.Snapshot<?>> item : compositeMonitor.mMetrics) {
            Assert.assertNull(compositeMonitor.mBgnSnapshots.get(item));
        }

        compositeMonitor.configureSnapshots();

        for (Class<? extends MonitorFeature.Snapshot<?>> item : compositeMonitor.mMetrics) {
            Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(item));
            Assert.assertNull(compositeMonitor.mDeltas.get(item));
        }

        compositeMonitor.configureDeltas();

        for (Class<? extends MonitorFeature.Snapshot<?>> item : compositeMonitor.mMetrics) {
            Assert.assertNotNull(compositeMonitor.mDeltas.get(item));
        }
    }
}
