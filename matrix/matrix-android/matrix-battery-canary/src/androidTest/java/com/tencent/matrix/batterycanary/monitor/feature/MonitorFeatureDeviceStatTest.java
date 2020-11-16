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
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.CpuFreqSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureDeviceStatTest {
    static final String TAG = "Matrix.test.MonitorFeatureDeviceStatTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
    }

    @After
    public void shutDown() {
    }

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(JiffiesMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }

    @Test
    public void testGetCpuFreqSnapshot() throws InterruptedException {
        final DeviceStatMonitorFeature feature = new DeviceStatMonitorFeature();
        feature.configure(mockMonitor());
        CpuFreqSnapshot bgn = feature.currentCpuFreq();
        Assert.assertNotNull(bgn);
        Assert.assertFalse(bgn.isDelta);
        Thread.sleep(100);
        CpuFreqSnapshot end = feature.currentCpuFreq();
        Assert.assertNotNull(end);
        Assert.assertFalse(end.isDelta);

        Delta<CpuFreqSnapshot> diff = end.diff(bgn);
        Assert.assertNotNull(diff);
        Assert.assertTrue(diff.dlt.isDelta);
        Assert.assertTrue(diff.during >= 100L);
        Assert.assertSame(bgn, diff.bgn);
        Assert.assertSame(end, diff.end);

        Assert.assertEquals(end.time, bgn.time + diff.during);
        for (int i = 0; i < diff.dlt.cpuFreq.length; i++) {
            int cpuDlt = diff.dlt.cpuFreq[i];
            int cpuBgn = diff.bgn.cpuFreq[i];
            int cpuEnd = diff.end.cpuFreq[i];
            Assert.assertEquals(cpuEnd + "-" + cpuBgn + "=" + cpuDlt, cpuEnd, cpuBgn + cpuDlt);
            Assert.assertEquals(
                    (int) diff.dlt.cpuFreqs.getList().get(i).get(),
                    diff.end.cpuFreqs.getList().get(i).get() - diff.bgn.cpuFreqs.getList().get(i).get()
            );
        }
    }

    @Test
    public void testGetBatteryTmpSnapshot() throws InterruptedException {
        final DeviceStatMonitorFeature feature = new DeviceStatMonitorFeature();
        feature.configure(mockMonitor());
        DeviceStatMonitorFeature.BatteryTmpSnapshot bgn = feature.currentBatteryTemperature(mContext);
        Assert.assertNotNull(bgn);
        Assert.assertFalse(bgn.isDelta);
        Thread.sleep(100);
        DeviceStatMonitorFeature.BatteryTmpSnapshot end = feature.currentBatteryTemperature(mContext);
        Assert.assertNotNull(end);
        Assert.assertFalse(end.isDelta);

        Delta<DeviceStatMonitorFeature.BatteryTmpSnapshot> diff = end.diff(bgn);
        Assert.assertNotNull(diff);
        Assert.assertTrue(diff.dlt.isDelta);
        Assert.assertTrue(diff.during >= 100L);
        Assert.assertSame(bgn, diff.bgn);
        Assert.assertSame(end, diff.end);
        Assert.assertEquals(end.time, bgn.time + diff.during);
        Assert.assertEquals(end.temperature, bgn.temperature + diff.dlt.temperature);
        Assert.assertEquals((int) diff.dlt.temp.get(), diff.end.temp.get() - diff.bgn.temp.get());
}
}
