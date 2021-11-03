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

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class CompositorTest {
    static final String TAG = "Matrix.test.CompositorTest";

    Context mContext;

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
        compositeMonitor.start();
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mBgnSnapshots.get(CpuStatFeature.CpuStateSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStatFeature.CpuStateSnapshot.class));
        compositeMonitor.finish();
        Assert.assertNotNull(compositeMonitor.mDeltas.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStatFeature.CpuStateSnapshot.class));

        compositeMonitor.clear();
        compositeMonitor
                .metric(JiffiesMonitorFeature.JiffiesSnapshot.class)
                .metric(CpuStatFeature.CpuStateSnapshot.class);
        compositeMonitor.start();
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(CpuStatFeature.CpuStateSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(JiffiesMonitorFeature.JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStatFeature.CpuStateSnapshot.class));
        compositeMonitor.finish();
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

        compositeMonitor.start();

        for (Class<? extends MonitorFeature.Snapshot<?>> item : compositeMonitor.mMetrics) {
            Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(item));
            Assert.assertNull(compositeMonitor.mDeltas.get(item));
        }

        compositeMonitor.finish();

        for (Class<? extends MonitorFeature.Snapshot<?>> item : compositeMonitor.mMetrics) {
            Assert.assertNotNull(compositeMonitor.mDeltas.get(item));
        }
    }

    @Test
    public void testPutDelta() {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();

        CompositeMonitors compositeMonitor = new CompositeMonitors(monitor);
        compositeMonitor.putDelta(AbsTaskMonitorFeature.TaskJiffiesSnapshot.class, Mockito.mock(MonitorFeature.Snapshot.Delta.class));
        Assert.assertNotNull(compositeMonitor.getDelta(AbsTaskMonitorFeature.TaskJiffiesSnapshot.class));
        compositeMonitor.putDelta(InternalMonitorFeature.InternalSnapshot.class, Mockito.mock(MonitorFeature.Snapshot.Delta.class));
        Assert.assertNotNull(compositeMonitor.getDeltaRaw(InternalMonitorFeature.InternalSnapshot.class));
    }

    @Test
    public void testGetCpuLoad() {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();

        CompositeMonitors compositeMonitor = new CompositeMonitors(monitor);
        Assert.assertEquals(-1, compositeMonitor.getCpuLoad());
        compositeMonitor.metric(JiffiesMonitorFeature.JiffiesSnapshot.class);
        compositeMonitor.start();
        compositeMonitor.finish();
        Assert.assertEquals(-1, compositeMonitor.getCpuLoad());

        compositeMonitor.metric(CpuStatFeature.CpuStateSnapshot.class);
        compositeMonitor.start();
        compositeMonitor.finish();
        Assert.assertTrue(compositeMonitor.getCpuLoad() >= 0 && compositeMonitor.getCpuLoad() <= BatteryCanaryUtil.getCpuCoreNum() * 100);
    }

    @Test
    public void testSampling() throws InterruptedException {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();

        CompositeMonitors compositeMonitor = new CompositeMonitors(monitor);
        Assert.assertNull(compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class));
        Assert.assertNull(compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class));
        compositeMonitor.sample(DeviceStatMonitorFeature.BatteryTmpSnapshot.class, 1L);
        compositeMonitor.sample(DeviceStatMonitorFeature.CpuFreqSnapshot.class, 10L);

        compositeMonitor.start();
        Thread.sleep(100L);
        compositeMonitor.finish();

        Assert.assertNotNull(compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class));
        Assert.assertEquals(1L, compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class).interval);
        Assert.assertTrue(compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class).duringMillis >= 100L);

        Assert.assertNotNull(compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class));
        Assert.assertEquals(10L, compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class).interval);
        Assert.assertTrue(compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class).duringMillis >= 100L);

        Assert.assertTrue(compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class).count > compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class).count);
    }
}
