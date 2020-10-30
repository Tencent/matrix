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

import android.content.Context;
import android.os.IBinder;
import android.os.PowerManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicInteger;

import static org.mockito.Mockito.mock;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureWakeLockTest {
    static final String tag = "Matrix.test.MonitorFeatureWakeLockTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
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
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }

    @Test
    public void testWakeLockRequired() throws InterruptedException {
        WakeLockMonitorFeature feature = new WakeLockMonitorFeature();
        feature.configure(mockMonitor());
        feature.mOverTimeMillis = Long.MAX_VALUE;
        Assert.assertEquals(0, feature.mWorkingWakeLocks.size());
        Assert.assertEquals(0, feature.mFinishedWakeLockRecords.size());
        WakeLockMonitorFeature.WakeLockInfo wakeLockInfo = feature.currentWakeLocksInfo();
        Assert.assertEquals(0, wakeLockInfo.totalWakeLockCount);
        Assert.assertEquals(0, wakeLockInfo.totalWakeLockTime);
        Assert.assertEquals(0, wakeLockInfo.totalWakeLockRecords.size());

        for (int i = 0; i < 100; i++) {
            IBinder mockToken = mock(IBinder.class);
            int flag = PowerManager.ACQUIRE_CAUSES_WAKEUP;
            String pkg = mContext.getPackageName();
            feature.onAcquireWakeLock(mockToken, flag, tag, pkg, null, null);
            Assert.assertEquals(0, feature.mFinishedWakeLockRecords.size());
            Assert.assertEquals(i + 1, feature.mWorkingWakeLocks.size());
        }

        Assert.assertEquals(0, feature.mFinishedWakeLockRecords.size());
        Assert.assertEquals(100, feature.mWorkingWakeLocks.size());

        ArrayList<WakeLockMonitorFeature.WakeLockTrace> workingList = new ArrayList<>(feature.mWorkingWakeLocks.values());
        for (int i = 0; i < workingList.size(); i++) {
            IBinder mockToken = workingList.get(i).token;
            int flag = workingList.get(i).record.flags;
            feature.onReleaseWakeLock(mockToken, flag);
            Assert.assertEquals(i + 1, feature.mFinishedWakeLockRecords.size());
            Assert.assertEquals(workingList.size() - (i + 1), feature.mWorkingWakeLocks.size());
            for (WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord item : feature.mFinishedWakeLockRecords) {
                Assert.assertTrue(item.isFinished());
                Assert.assertTrue(item.getLockingTimeMillis() > 0L);
            }
            for (WakeLockMonitorFeature.WakeLockTrace item : feature.mWorkingWakeLocks.values()) {
                Assert.assertFalse(item.isFinished());
                Assert.assertTrue(item.record.getLockingTimeMillis() >= 0L);
            }
        }

        Assert.assertEquals(100, feature.mFinishedWakeLockRecords.size());
        Assert.assertEquals(0, feature.mWorkingWakeLocks.size());
    }

    @Test
    public void testWakeLockRequiredConcurrent() throws InterruptedException {
        final WakeLockMonitorFeature feature = new WakeLockMonitorFeature();
        feature.configure(mockMonitor());
        feature.mOverTimeMillis = Long.MAX_VALUE;
        Assert.assertEquals(0, feature.mWorkingWakeLocks.size());
        Assert.assertEquals(0, feature.mFinishedWakeLockRecords.size());
        WakeLockMonitorFeature.WakeLockInfo wakeLockInfo = feature.currentWakeLocksInfo();
        Assert.assertEquals(0, wakeLockInfo.totalWakeLockCount);
        Assert.assertEquals(0, wakeLockInfo.totalWakeLockTime);
        Assert.assertEquals(0, wakeLockInfo.totalWakeLockRecords.size());

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
        Assert.assertEquals(0, feature.mFinishedWakeLockRecords.size());
        Assert.assertEquals(100, feature.mWorkingWakeLocks.size());

        final ArrayList<WakeLockMonitorFeature.WakeLockTrace> workingList = new ArrayList<>(feature.mWorkingWakeLocks.values());
        threads = new ArrayList<>();
        for (int i = 0; i < workingList.size(); i++) {
            final IBinder mockToken = workingList.get(i).token;
            final int flag = workingList.get(i).record.flags;
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    synchronized (feature.mFinishedWakeLockRecords) {
                        feature.onReleaseWakeLock(mockToken, flag);
                        for (WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord item : feature.mFinishedWakeLockRecords) {
                            Assert.assertTrue(item.isFinished());
                            Assert.assertTrue(item.getLockingTimeMillis() > 0L);
                        }
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
        Assert.assertEquals(100, feature.mFinishedWakeLockRecords.size());
        Assert.assertEquals(0, feature.mWorkingWakeLocks.size());
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
                    public void onWakeLockTimeout(int warningCount, WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord record) {
                        super.onWakeLockTimeout(warningCount, record);
                        overTimeCount.incrementAndGet();
                    }
                })
                .build();
        final WakeLockMonitorFeature feature = new WakeLockMonitorFeature();
        feature.configure(new BatteryMonitorCore(config));

        IBinder mockToken = mock(IBinder.class);
        int flag = PowerManager.ACQUIRE_CAUSES_WAKEUP;
        String pkg = mContext.getPackageName();
        feature.onAcquireWakeLock(mockToken, flag, tag, pkg, null, null);

        Assert.assertEquals(0, overTimeCount.get());
        for (int i = 0; i < 10; i++) {
            final int expected = i;
            feature.handler.post(new Runnable() {
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
                    public void onWakeLockTimeout(int warningCount, WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord record) {
                        super.onWakeLockTimeout(warningCount, record);
                        overTimeCount.incrementAndGet();
                    }
                })
                .build();
        final WakeLockMonitorFeature feature = new WakeLockMonitorFeature();
        feature.configure(new BatteryMonitorCore(config));

        for (int i = 0; i < concurrentNum; i++) {
            IBinder mockToken = mock(IBinder.class);
            int flag = PowerManager.ACQUIRE_CAUSES_WAKEUP;
            String pkg = mContext.getPackageName();
            feature.onAcquireWakeLock(mockToken, flag, tag, pkg, null, null);
        }

        Assert.assertEquals(0, overTimeCount.get());
        for (int i = 0; i < 10; i++) {
            final int expected = i;
            feature.handler.post(new Runnable() {
                @Override
                public void run() {
                    Assert.assertTrue(overTimeCount.get() >= (expected - 1) * concurrentNum);
                    Assert.assertTrue(overTimeCount.get() <= (expected + 1) * concurrentNum);
                }
            });
            Thread.sleep(timeoutMillis);
        }
    }
}
