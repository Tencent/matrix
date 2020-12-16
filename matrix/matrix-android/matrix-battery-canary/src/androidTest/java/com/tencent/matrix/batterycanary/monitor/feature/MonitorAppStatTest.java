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
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;


@RunWith(AndroidJUnit4.class)
public class MonitorAppStatTest {
    static final String TAG = "Matrix.test.MonitorAppStatTest";

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
                .enable(WakeLockMonitorFeature.class)
                .enable(DeviceStatMonitorFeature.class)
                .enable(AlarmMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }

    @Test
    public void testGetCurrentSnapshot() throws InterruptedException {
        AppStatMonitorFeature feature = new AppStatMonitorFeature();
        feature.configure(mockMonitor());
        feature.onTurnOn();

        Assert.assertEquals(1, feature.mStampList.size());
        Assert.assertEquals(1, feature.mStampList.get(0).appStat);

        Thread.sleep(100);
        AppStatMonitorFeature.AppStatSnapshot snapshot = feature.currentAppStatSnapshot();
        Assert.assertNotNull(snapshot);
        Assert.assertEquals(2, feature.mStampList.size());
        Assert.assertEquals(2, feature.mStampList.get(0).appStat);
        Assert.assertTrue(snapshot.uptime.get() >= 100L);
        Assert.assertTrue(snapshot.fgRatio.get() > 0L);
        Assert.assertEquals(0L, (long) snapshot.bgRatio.get());
        Assert.assertEquals(0L, (long) snapshot.fgSrvRatio.get());
    }

    @Test
    public void testGetCurrentSnapshotWithBg() throws InterruptedException {
        AppStatMonitorFeature feature = new AppStatMonitorFeature();
        feature.configure(mockMonitor());
        feature.onTurnOn();

        Thread.sleep(100);
        AppStatMonitorFeature.AppStatSnapshot snapshot = feature.currentAppStatSnapshot();
        Assert.assertNotNull(snapshot);

        Assert.assertEquals(2, feature.mStampList.size());
        Assert.assertEquals(2, feature.mStampList.get(0).appStat);
        Assert.assertTrue(snapshot.uptime.get() >= 100L);
        Assert.assertTrue(snapshot.fgRatio.get() > 99);
        Assert.assertEquals(0L, (long) snapshot.bgRatio.get());
        Assert.assertEquals(0L, (long) snapshot.fgSrvRatio.get());

        feature.onForeground(false);
        Thread.sleep(100);
        snapshot = feature.currentAppStatSnapshot();
        Assert.assertNotNull(snapshot);
        Assert.assertEquals(4, feature.mStampList.size());
        Assert.assertEquals(2, feature.mStampList.get(0).appStat);
        Assert.assertTrue(snapshot.uptime.get() >= 200L);
        Assert.assertTrue(snapshot.fgRatio.get() > 0L && snapshot.fgRatio.get() <= 50);
        Assert.assertTrue(snapshot.bgRatio.get() > 0L && snapshot.fgRatio.get() <= 50);
        Assert.assertEquals(0L, (long) snapshot.fgSrvRatio.get());

        AppStatMonitorFeature.Stamp stamp = new AppStatMonitorFeature.Stamp(3);
        feature.mStampList.add(0, stamp);
        Assert.assertEquals(5, feature.mStampList.size());
        Assert.assertEquals(3, feature.mStampList.get(0).appStat);

        Thread.sleep(100);

        snapshot = feature.currentAppStatSnapshot();
        Assert.assertNotNull(snapshot);
        Assert.assertEquals(6, feature.mStampList.size());
        Assert.assertEquals(2, feature.mStampList.get(0).appStat);
        Assert.assertTrue(snapshot.uptime.get() >= 300L);
        Assert.assertTrue(snapshot.fgRatio.get() > 0L && snapshot.fgRatio.get() <= 35);
        Assert.assertTrue(snapshot.bgRatio.get() > 0L && snapshot.fgRatio.get() <= 35);
        Assert.assertTrue(snapshot.fgSrvRatio.get() > 0L && snapshot.fgSrvRatio.get() <= 35);
    }

    @Test
    public void testGetCurrentSnapshotWithBgAndWindowMillis() throws InterruptedException {
        List<AppStatMonitorFeature.Stamp> stampList = new ArrayList<>();
        stampList.add(0, new AppStatMonitorFeature.Stamp(1));
        Thread.sleep(100);
        stampList.add(0, new AppStatMonitorFeature.Stamp(2));
        Thread.sleep(100);
        stampList.add(0, new AppStatMonitorFeature.Stamp(1));
        Thread.sleep(100);
        stampList.add(0, new AppStatMonitorFeature.Stamp(3));
        Thread.sleep(100);
        stampList.add(0, new AppStatMonitorFeature.Stamp(1));

        AppStatMonitorFeature.AppStatSnapshot snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 0L);
        Assert.assertEquals(400, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(50, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(25, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(25, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, Long.MIN_VALUE);
        Assert.assertEquals(400, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(50, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(25, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(25, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());

        // last 50 millis
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 50L);
        Assert.assertEquals(50, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(0, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(0, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(100, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 100L);
        Assert.assertEquals(100, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(0, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(0, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(100, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 150L);
        Assert.assertEquals(150, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(33.3, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(0, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(66.6, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 200L);
        Assert.assertEquals(200, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(50, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(0, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(50, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 250L);
        Assert.assertEquals(250, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(100 * 100/250f, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 50/250f, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/250f, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 300L);
        Assert.assertEquals(300f, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(100 * 100/300f, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/300f, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/300f, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 350L);
        Assert.assertEquals(350f, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(100 * 150/350f, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/350f, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/350f, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 400L);
        Assert.assertEquals(400f, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(100 * 200/400f, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/400f, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/400f, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertTrue(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, 500L);
        Assert.assertEquals(400f, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(100 * 200/400f, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/400f, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/400f, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertFalse(snapshot.isValid());
        snapshot = AppStatMonitorFeature.configureSnapshot(stampList, Long.MAX_VALUE);
        Assert.assertEquals(400f, snapshot.uptime.get().floatValue(), 10);
        Assert.assertEquals(100 * 200/400f, snapshot.fgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/400f, snapshot.bgRatio.get().floatValue(), 1);
        Assert.assertEquals(100 * 100/400f, snapshot.fgSrvRatio.get().floatValue(), 1);
        Assert.assertFalse(snapshot.isValid());
    }

    @Test
    public void testEmptyListOps() {
        List<Object> emptyList = Collections.emptyList();
        try {
            emptyList.add(0, null);
            Assert.fail("should failed");
        } catch (Exception ignored) {
        }
    }

    @Test
    public void testConcurrent() throws InterruptedException {
        final AppStatMonitorFeature feature = new AppStatMonitorFeature();
        feature.configure(mockMonitor());

        List<Thread> threadList = new ArrayList<>();
        for (int i = 0; i < 100; i++) {
            final int finalI = i;
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    feature.onForeground( finalI % 2 == 0);
                }
            });
            thread.start();
            threadList.add(thread);
        }

        for (int i = 0; i < 100; i++) {
            final int finalI = i;
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                   if ( finalI % 3 == 0) {
                       feature.onTurnOff();
                   } else {
                       feature.onTurnOn();
                   }
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
