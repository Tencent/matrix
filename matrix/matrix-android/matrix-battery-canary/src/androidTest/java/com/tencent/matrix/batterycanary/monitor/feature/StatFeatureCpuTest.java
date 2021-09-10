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
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature.CpuStateSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class StatFeatureCpuTest {
    static final String TAG = "Matrix.test.StatFeatureCpuTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        if (!Matrix.isInstalled()) {
            Matrix.init(new Matrix.Builder(((Application) mContext.getApplicationContext())).build());
        }
    }

    @After
    public void shutDown() {
    }

    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(CpuStatFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }

    @Test
    public void testGetCpuStatesSnapshot() throws InterruptedException {
        CpuStatFeature feature = new CpuStatFeature();
        feature.configure(mockMonitor());
        feature.onTurnOn();

        Assert.assertTrue(feature.isSupported());
        CpuStateSnapshot cpuStateSnapshot = feature.currentCpuStateSnapshot();
        Assert.assertFalse(cpuStateSnapshot.isDelta);
        Assert.assertTrue(cpuStateSnapshot.isValid());
        Assert.assertTrue(cpuStateSnapshot.totalJiffies() > 0);
    }

    @Test
    public void testGetCpuStatesSnapshotDelta() throws InterruptedException {
        CpuStatFeature feature = new CpuStatFeature();
        feature.configure(mockMonitor());
        feature.onTurnOn();

        Assert.assertTrue(feature.isSupported());

        CpuStateSnapshot bgn = feature.currentCpuStateSnapshot();
        Thread.sleep(1000L);
        CpuStateSnapshot end = feature.currentCpuStateSnapshot();
        Delta<CpuStateSnapshot> delta = end.diff(bgn);

        Assert.assertTrue(delta.bgn.isValid());
        Assert.assertTrue(delta.end.isValid());
        Assert.assertTrue(delta.dlt.isValid());
        Assert.assertTrue(delta.dlt.isDelta);
        Assert.assertTrue(delta.end.totalJiffies() >= delta.bgn.totalJiffies());
        Assert.assertEquals(delta.dlt.totalJiffies(), end.totalJiffies() - bgn.totalJiffies());
    }

    @Test
    public void testConcurrent() throws InterruptedException {
        final CpuStatFeature feature = new CpuStatFeature();
        feature.configure(mockMonitor());
        feature.onTurnOn();

        List<Thread> threadList = new ArrayList<>();
        for (int i = 0; i < 100; i++) {
            final int finalI = i;
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    feature.currentCpuStateSnapshot();
                }
            });
            thread.start();
            threadList.add(thread);
        }
        for (Thread item : threadList) {
            item.join();
        }
    }
}
