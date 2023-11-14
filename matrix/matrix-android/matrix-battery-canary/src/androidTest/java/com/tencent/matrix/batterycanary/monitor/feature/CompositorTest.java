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
import android.os.SystemClock;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature.CpuStateSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
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
        Assert.assertNull(compositeMonitor.mBgnSnapshots.get(JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mBgnSnapshots.get(CpuStateSnapshot.class));
        compositeMonitor.metric(JiffiesSnapshot.class);
        compositeMonitor.start();
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mBgnSnapshots.get(CpuStateSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStateSnapshot.class));
        compositeMonitor.finish();
        Assert.assertNotNull(compositeMonitor.mDeltas.get(JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStateSnapshot.class));

        compositeMonitor.clear();
        compositeMonitor
                .metric(JiffiesSnapshot.class)
                .metric(CpuStateSnapshot.class);
        compositeMonitor.start();
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(JiffiesSnapshot.class));
        Assert.assertNotNull(compositeMonitor.mBgnSnapshots.get(CpuStateSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(JiffiesSnapshot.class));
        Assert.assertNull(compositeMonitor.mDeltas.get(CpuStateSnapshot.class));
        compositeMonitor.finish();
        Assert.assertNotNull(compositeMonitor.mDeltas.get(JiffiesSnapshot.class));
        Assert.assertNotNull(compositeMonitor.mDeltas.get(CpuStateSnapshot.class));
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
        compositeMonitor.putDelta(AbsTaskMonitorFeature.TaskJiffiesSnapshot.class, Mockito.mock(Delta.class));
        Assert.assertNotNull(compositeMonitor.getDelta(AbsTaskMonitorFeature.TaskJiffiesSnapshot.class));
        compositeMonitor.putDelta(InternalMonitorFeature.InternalSnapshot.class, Mockito.mock(Delta.class));
        Assert.assertNotNull(compositeMonitor.getDeltaRaw(InternalMonitorFeature.InternalSnapshot.class));
    }

    @Test
    public void testGetCpuLoad() throws InterruptedException {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();

        CompositeMonitors compositeMonitor = new CompositeMonitors(monitor);
        Assert.assertEquals(-1, compositeMonitor.getCpuLoad());

        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                }
            }
        }, "TEST");
        thread.start();

        compositeMonitor.metric(JiffiesSnapshot.class);
        compositeMonitor.start();
        Thread.sleep(1000L);
        compositeMonitor.finish();
        int cpuLoadR = compositeMonitor.getCpuLoad();
        Assert.assertTrue(cpuLoadR >= 0 && cpuLoadR <= BatteryCanaryUtil.getCpuCoreNum() * 100);

        compositeMonitor.metric(CpuStateSnapshot.class);
        compositeMonitor.start();
        Thread.sleep(1000L);
        compositeMonitor.finish();
        int devCpuLoad = compositeMonitor.getDevCpuLoad();
        Assert.assertTrue("devCpuLoad: " + devCpuLoad, devCpuLoad >= 0 && devCpuLoad <= BatteryCanaryUtil.getCpuCoreNum() * 100);

        Assert.assertEquals(devCpuLoad, cpuLoadR, 10);

        compositeMonitor = new CompositeMonitors(monitor);
        Assert.assertFalse(compositeMonitor.mMetrics.contains(JiffiesSnapshot.class));
        Assert.assertFalse(compositeMonitor.mMetrics.contains(CpuStateSnapshot.class));
        compositeMonitor.metricCpuLoad();
        compositeMonitor.start();
        long wallTimeBgn = System.currentTimeMillis();
        long upTimeBgn = SystemClock.uptimeMillis();
        Thread.sleep(10 * 1000L);
        compositeMonitor.finish();
        long wallTimeEnd = System.currentTimeMillis();
        long upTimeEnd = SystemClock.uptimeMillis();
        devCpuLoad = compositeMonitor.getDevCpuLoad();
        Assert.assertTrue("devCpuLoad: " + devCpuLoad,devCpuLoad >= 0 && devCpuLoad <= BatteryCanaryUtil.getCpuCoreNum() * 100);

        long wallTimeDelta = wallTimeEnd - wallTimeBgn;
        long uptimeDelta = upTimeEnd - upTimeBgn;
        Assert.assertEquals(wallTimeDelta, uptimeDelta, 100L);

        Delta<JiffiesSnapshot> appJiffies = compositeMonitor.getDelta(JiffiesSnapshot.class);
        Assert.assertNotNull(appJiffies);
        Delta<CpuStateSnapshot> cpuJiffies = compositeMonitor.getDelta(CpuStateSnapshot.class);
        Assert.assertNotNull(cpuJiffies);
        Assert.assertEquals((uptimeDelta / 10) * BatteryCanaryUtil.getCpuCoreNum(), cpuJiffies.dlt.totalCpuJiffies(), 20L);

        cpuLoadR = (int) ((appJiffies.dlt.totalJiffies.get() / (uptimeDelta / 10f)) * 100);
        Assert.assertTrue("cpuLoadR: " + cpuLoadR, cpuLoadR >= 0 && cpuLoadR <= BatteryCanaryUtil.getCpuCoreNum() * 100);
        Assert.assertEquals(devCpuLoad, cpuLoadR, 10);
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

    @Test
    public void testSamplingStop() throws InterruptedException {
        final BatteryMonitorCore monitor = mockMonitor();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
        Matrix.with().getPlugins().add(plugin);
        monitor.enableForegroundLoopCheck(true);
        monitor.start();

        long interval = 100L;
        long samplingTime = 1000L;
        CompositeMonitors compositeMonitor = new CompositeMonitors(monitor);
        compositeMonitor.sample(DeviceStatMonitorFeature.BatteryTmpSnapshot.class, interval);
        compositeMonitor.sample(DeviceStatMonitorFeature.CpuFreqSnapshot.class, interval);
        compositeMonitor.sample(DeviceStatMonitorFeature.ChargeWattageSnapshot.class, interval);
        compositeMonitor.sample(CpuStatFeature.CpuStateSnapshot.class, interval);
        compositeMonitor.sample(JiffiesMonitorFeature.UidJiffiesSnapshot.class, interval);

        compositeMonitor.start();
        Thread.sleep(samplingTime);
        compositeMonitor.finish();

        Thread.sleep(4000L);
        Assert.assertEquals(samplingTime/interval, compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.BatteryTmpSnapshot.class).count, 2);
        Assert.assertEquals(samplingTime/interval, compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.CpuFreqSnapshot.class).count, 2);
        Assert.assertEquals(samplingTime/interval, compositeMonitor.getSamplingResult(DeviceStatMonitorFeature.ChargeWattageSnapshot.class).count, 2);
        Assert.assertEquals(samplingTime/interval, compositeMonitor.getSamplingResult(CpuStatFeature.CpuStateSnapshot.class).count, 3);
        Assert.assertEquals(samplingTime/interval, compositeMonitor.getSamplingResult(JiffiesMonitorFeature.UidJiffiesSnapshot.class).count, 2);
    }
}
