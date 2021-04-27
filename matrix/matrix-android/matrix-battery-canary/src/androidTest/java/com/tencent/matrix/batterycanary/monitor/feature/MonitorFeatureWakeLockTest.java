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
import android.os.IBinder;
import android.os.PowerManager;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Differ.ListDiffer;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.BeanEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTracing;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

import static org.mockito.Mockito.mock;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureWakeLockTest {
    static final String tag = "Matrix.test.MonitorFeatureWakeLockTest";

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
                .enable(WakeLockMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .enableAmsHook(true)
                .foregroundLoopCheckTime(1000)
                .build();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        plugin.init(((Application)mContext.getApplicationContext()), null);
        Matrix.with().getPlugins().add(plugin);
        return new BatteryMonitorCore(config);
    }

    @Test
    public void testWakeLockRequired() throws InterruptedException {
        WakeLockMonitorFeature feature = new WakeLockMonitorFeature();
        feature.configure(mockMonitor());
        feature.onTurnOn();

        feature.mOverTimeMillis = Integer.MAX_VALUE;
        Assert.assertEquals(0, feature.mWorkingWakeLocks.size());
        Assert.assertEquals(0, feature.mWakeLockTracing.getTotalCount());
        WakeLockMonitorFeature.WakeLockSnapshot wakeLockSnapshot = feature.currentWakeLocks();
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockCount.get().intValue());
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockTime.get().longValue());
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockRecords.getList().size());

        for (int i = 0; i < 100; i++) {
            IBinder mockToken = mock(IBinder.class);
            int flag = PowerManager.ACQUIRE_CAUSES_WAKEUP;
            String pkg = mContext.getPackageName();
            feature.onAcquireWakeLock(mockToken, flag, tag, pkg, null, null);
            Assert.assertEquals(0, feature.mWakeLockTracing.getTotalCount());
            Assert.assertEquals(i + 1, feature.mWorkingWakeLocks.size());
        }

        Assert.assertEquals(0, feature.mWakeLockTracing.getTotalCount());
        Assert.assertEquals(100, feature.mWorkingWakeLocks.size());
        wakeLockSnapshot = feature.currentWakeLocks();
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockCount.get().intValue());
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockRecords.getList().size());

        ArrayList<WakeLockMonitorFeature.WakeLockTrace> workingList = new ArrayList<>(feature.mWorkingWakeLocks.values());
        for (int i = 0; i < workingList.size(); i++) {
            IBinder mockToken = workingList.get(i).token;
            int flag = workingList.get(i).record.flags;
            feature.onReleaseWakeLock(mockToken, flag);
            Assert.assertEquals(i + 1, feature.mWakeLockTracing.getTotalCount());
            Assert.assertEquals(workingList.size() - (i + 1), feature.mWorkingWakeLocks.size());
            for (WakeLockMonitorFeature.WakeLockTrace item : feature.mWorkingWakeLocks.values()) {
                Assert.assertFalse(item.isFinished());
                Assert.assertTrue(item.record.getLockingTimeMillis() >= 0L);
            }
        }

        Assert.assertEquals(100, feature.mWakeLockTracing.getTotalCount());
        Assert.assertEquals(0, feature.mWorkingWakeLocks.size());
        wakeLockSnapshot = feature.currentWakeLocks();
        Assert.assertEquals(100, wakeLockSnapshot.totalWakeLockCount.get().intValue());
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockRecords.getList().size());
    }

    @Test
    public void testWakeLockRequiredConcurrent() throws InterruptedException {
        if (TestUtils.isAssembleTest()) return;
        final WakeLockMonitorFeature feature = new WakeLockMonitorFeature();
        feature.configure(mockMonitor());
        feature.mOverTimeMillis = Integer.MAX_VALUE;
        Assert.assertEquals(0, feature.mWorkingWakeLocks.size());
        Assert.assertEquals(0, feature.mWakeLockTracing.getTotalCount());
        WakeLockMonitorFeature.WakeLockSnapshot wakeLockSnapshot = feature.currentWakeLocks();
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockCount.get().intValue());
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockTime.get().longValue());
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockRecords.getList().size());

        ArrayList<Thread> threads = new ArrayList<>();
        for (int i = 0; i < 100; i++) {
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    IBinder mockToken = mock(IBinder.class);
                    int flag = PowerManager.ACQUIRE_CAUSES_WAKEUP;
                    String pkg = mContext.getPackageName();
                    feature.onAcquireWakeLock(mockToken, flag, tag, pkg, null, null);
                }
            });
            thread.start();
            threads.add(thread);
        }

        for (Thread item : threads) {
            item.join();
        }
        Assert.assertEquals(0, feature.mWakeLockTracing.getTotalCount());
        Assert.assertEquals(100, feature.mWorkingWakeLocks.size());
        wakeLockSnapshot = feature.currentWakeLocks();
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockCount.get().intValue());
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockRecords.getList().size());

        final ArrayList<WakeLockMonitorFeature.WakeLockTrace> workingList = new ArrayList<>(feature.mWorkingWakeLocks.values());
        threads = new ArrayList<>();
        for (int i = 0; i < workingList.size(); i++) {
            final IBinder mockToken = workingList.get(i).token;
            final int flag = workingList.get(i).record.flags;
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    synchronized (feature.mWorkingWakeLocks) {
                        feature.onReleaseWakeLock(mockToken, flag);
                        for (WakeLockMonitorFeature.WakeLockTrace item : feature.mWorkingWakeLocks.values()) {
                            Assert.assertFalse(item.isFinished());
                            Assert.assertTrue(item.record.getLockingTimeMillis() > 0L);
                        }
                    }
                }
            });
            thread.start();
            threads.add(thread);
        }

        for (Thread item : threads) {
            item.join();
        }
        Assert.assertEquals(100, feature.mWakeLockTracing.getTotalCount());
        Assert.assertEquals(0, feature.mWorkingWakeLocks.size());
        wakeLockSnapshot = feature.currentWakeLocks();
        Assert.assertEquals(100, wakeLockSnapshot.totalWakeLockCount.get().intValue());
        Assert.assertEquals(0, wakeLockSnapshot.totalWakeLockRecords.getList().size());
    }

    @Test
    public void testAcquiredTimeOut() throws InterruptedException {
        final AtomicInteger overTimeCount = new AtomicInteger();
        long timeoutMillis = 200L;
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(WakeLockMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(timeoutMillis)
                .setCallback(new BatteryMonitorCallback.BatteryPrinter() {
                    @Override
                    public void onWakeLockTimeout(int warningCount, WakeLockRecord record) {
                        super.onWakeLockTimeout(warningCount, record);
                        overTimeCount.incrementAndGet();
                    }
                })
                .build();
        final WakeLockMonitorFeature feature = new WakeLockMonitorFeature();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        Matrix.with().getPlugins().add(plugin);
        BatteryMonitorCore monitorCore = plugin.core();
        feature.configure(monitorCore);
        feature.onTurnOn();

        IBinder mockToken = mock(IBinder.class);
        int flag = PowerManager.ACQUIRE_CAUSES_WAKEUP;
        String pkg = mContext.getPackageName();
        feature.onAcquireWakeLock(mockToken, flag, tag, pkg, null, null);

        Assert.assertEquals(0, overTimeCount.get());
        for (int i = 0; i < 10; i++) {
            final int expected = i;
            monitorCore.getHandler().post(new Runnable() {
                @Override
                public void run() {
                    Assert.assertTrue(overTimeCount.get() >= expected - 1);
                    Assert.assertTrue(overTimeCount.get() <= expected + 1);
                }
            });
            Thread.sleep(timeoutMillis);
        }
    }

    @Test
    public void testAcquiredTimeOutConcurrent() throws InterruptedException {
        final AtomicInteger overTimeCount = new AtomicInteger();
        long timeoutMillis = 200L;
        final int concurrentNum = 5;
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(WakeLockMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(timeoutMillis)
                .setCallback(new BatteryMonitorCallback.BatteryPrinter() {
                    @Override
                    public void onWakeLockTimeout(int warningCount, WakeLockRecord record) {
                        super.onWakeLockTimeout(warningCount, record);
                        overTimeCount.incrementAndGet();
                    }
                })
                .build();
        final WakeLockMonitorFeature feature = new WakeLockMonitorFeature();
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
        Matrix.with().getPlugins().add(plugin);
        BatteryMonitorCore monitorCore = plugin.core();
        feature.configure(monitorCore);
        feature.onTurnOn();

        for (int i = 0; i < concurrentNum; i++) {
            IBinder mockToken = mock(IBinder.class);
            int flag = PowerManager.ACQUIRE_CAUSES_WAKEUP;
            String pkg = mContext.getPackageName();
            feature.onAcquireWakeLock(mockToken, flag, tag, pkg, null, null);
        }

        Assert.assertEquals(0, overTimeCount.get());
        for (int i = 0; i < 10; i++) {
            final int expected = i;
            monitorCore.getHandler().post(new Runnable() {
                @Override
                public void run() {
                    Assert.assertTrue(overTimeCount.get() >= (expected - 1) * concurrentNum);
                    Assert.assertTrue(overTimeCount.get() <= (expected + 1) * concurrentNum);
                }
            });
            Thread.sleep(timeoutMillis);
        }
    }

    @Test
    public void testRecordListBenchmark() {
        if (TestUtils.isAssembleTest()) return;

        int round = 1000;

        long str = System.currentTimeMillis();
        List<BeanEntry<WakeLockRecord>> records = new ArrayList<>();
        for (int i = 0; i < round; i++) {
            WakeLockRecord mock = new WakeLockRecord("xxx", 0, "yyy", "zzz");
            records.add(BeanEntry.of(mock));
        }
        ListEntry<BeanEntry<WakeLockRecord>> bgn = ListEntry.of(records);
        for (int i = 0; i < round; i++) {
            WakeLockRecord mock = new WakeLockRecord("xxx", 0, "yyy", "zzz");
            records.add(BeanEntry.of(mock));
        }
        ListEntry<BeanEntry<WakeLockRecord>> end = ListEntry.of(records);
        ListDiffer.globalDiff(bgn, end);
        long differConsumed = System.currentTimeMillis() - str;

        str = System.currentTimeMillis();
        List<WakeLockRecord> bgnList = new ArrayList<>();
        for (int i = 0; i < round; i++) {
            WakeLockRecord mock = new WakeLockRecord("xxx", 0, "yyy", "zzz");
            bgnList.add((mock));
        }
        List<WakeLockRecord> endList = new ArrayList<>(bgnList);
        for (int i = 0; i < round; i++) {
            WakeLockRecord mock = new WakeLockRecord("xxx", 0, "yyy", "zzz");
            endList.add((mock));
        }
        List<WakeLockRecord> dltList = new ArrayList<>(bgnList);
        for (WakeLockRecord endItem : endList) {
            if (!bgnList.contains(endItem)) {
                dltList.add(endItem);
            }
        }
        long rawEqualsConsumed = System.currentTimeMillis() - str;

        Assert.fail("Time consumed: " + differConsumed + " vs " + rawEqualsConsumed + " millis");
    }


    @Test
    public void testWakeLockCountingBenchmark() {
        WakeLockTracing counting = new WakeLockTracing();

        int round = 10000;

        long str = System.currentTimeMillis();
        for (int i = 0; i < round; i++) {
            WakeLockRecord mock = new WakeLockRecord("xxx", 0, "yyy", "zzz");
            counting.add(mock);
        }
        WakeLockMonitorFeature.WakeLockSnapshot bgn = counting.getSnapshot();
        for (int i = 0; i < round; i++) {
            WakeLockRecord mock = new WakeLockRecord("xxx", 0, "yyy", "zzz");
            counting.add(mock);
        }
        WakeLockMonitorFeature.WakeLockSnapshot end = counting.getSnapshot();
        end.diff(bgn);
        long differConsumed = System.currentTimeMillis() - str;

        Assert.assertFalse("Time consumed: " + differConsumed, differConsumed > 100L);
    }
}
