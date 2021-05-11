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
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.Process;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import android.text.TextUtils;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.AbsTaskMonitorFeature.TaskJiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.utils.TimeBreaker;
import com.tencent.matrix.trace.core.LooperMonitor;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureLooperTest {
    static final String TAG = "Matrix.test.LooperMonitorTest";

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

    @Test
    public void testLooperMonitor() throws InterruptedException {
        final AtomicBoolean hasStart = new AtomicBoolean();
        final AtomicBoolean hasFinish = new AtomicBoolean();

        HandlerThread handlerThread = new HandlerThread("looper-test");
        handlerThread.start();
        LooperMonitor looperMonitor = new LooperMonitor(handlerThread.getLooper());
        looperMonitor.addListener(new LooperMonitor.LooperDispatchListener() {
            @Override
            public boolean isValid() {
                return true;
            }

            @Override
            public void dispatchStart() {
                Assert.assertEquals("looper-test", Thread.currentThread().getName());
                if (!hasStart.get()) {
                    Assert.assertFalse(hasFinish.get());
                }
                hasStart.set(true);

            }

            @Override
            public void dispatchEnd() {
                Assert.assertEquals("looper-test", Thread.currentThread().getName());
                hasFinish.set(true);
                Assert.assertTrue(hasStart.get());

                synchronized (hasFinish) {
                    hasFinish.notifyAll();
                }
            }
        });

        synchronized (hasStart) {
            if (hasStart.get()) {
                Assert.assertTrue(hasFinish.get());
            } else {
                Assert.assertFalse(hasFinish.get());
            }
        }

        Handler handler = new Handler(handlerThread.getLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
            }
        });

        synchronized (hasFinish) {
            hasFinish.wait();
        }

        Assert.assertTrue(hasStart.get());
        Assert.assertTrue(hasFinish.get());
    }

    @Test
    public void testLooperMonitorV2() throws InterruptedException {
        final AtomicBoolean hasStart = new AtomicBoolean();
        final AtomicBoolean hasFinish = new AtomicBoolean();

        HandlerThread handlerThread = new HandlerThread("looper-test");
        handlerThread.start();
        LooperMonitor looperMonitor = new LooperMonitor(handlerThread.getLooper());
        looperMonitor.addListener(new LooperMonitor.LooperDispatchListener() {
            @Override
            public boolean isValid() {
                return true;
            }

            /**
             * >>>>> Dispatching to Handler (android.os.Handler) {b54e421} com.tencent.matrix.batterycanary.utils.LooperMonitorTest$TestTask@32d6046: 0
             */
            @Override
            public void onDispatchStart(String x) {
                super.onDispatchStart(x);
                if (!hasStart.get()) {
                    Assert.assertFalse(hasFinish.get());
                }
                hasStart.set(true);

                Assert.assertFalse(TextUtils.isEmpty(x));

                String symbolBgn = "} ";
                String symbolEnd = "@";
                Assert.assertTrue(x.contains(symbolBgn));
                Assert.assertTrue(x.contains(symbolEnd));

                int indexBgn = x.indexOf(symbolBgn);
                int indexEnd = x.lastIndexOf(symbolEnd);
                Assert.assertTrue(indexBgn < indexEnd);

                String taskName = x.substring(indexBgn + symbolBgn.length(), indexEnd);
                Assert.assertFalse(TextUtils.isEmpty(taskName));

                symbolBgn = "@";
                symbolEnd = ": ";
                Assert.assertTrue(x.contains(symbolBgn));
                Assert.assertTrue(x.contains(symbolEnd));

                indexBgn = x.indexOf(symbolBgn);
                indexEnd = x.lastIndexOf(symbolEnd);
                Assert.assertTrue(indexBgn < indexEnd);

                String hexString = x.substring(indexBgn + symbolBgn.length(), indexEnd);
                int hashcode = Integer.parseInt(hexString, 16);
                Assert.assertTrue(hashcode > 0);

                Assert.assertEquals(taskName, computeTaskName(x));
                Assert.assertEquals(hashcode, computeHashcode(x));
            }

            /**
             * <<<<< Finished to Handler (android.os.Handler) {b54e421} com.tencent.matrix.batterycanary.utils.LooperMonitorTest$TestTask@32d6046
             */
            @Override
            public void onDispatchEnd(String x) {
                super.onDispatchEnd(x);
                hasFinish.set(true);
                Assert.assertTrue(hasStart.get());

                synchronized (hasFinish) {
                    hasFinish.notifyAll();
                }

                Assert.assertFalse(TextUtils.isEmpty(x));

                String symbolBgn = "} ";
                String symbolEnd = "@";
                Assert.assertTrue(x.contains(symbolBgn));
                Assert.assertTrue(x.contains(symbolEnd));

                int indexBgn = x.indexOf(symbolBgn);
                int indexEnd = x.lastIndexOf(symbolEnd);
                Assert.assertTrue(indexBgn < indexEnd);

                String taskName = x.substring(indexBgn + symbolBgn.length(), indexEnd);
                Assert.assertFalse(TextUtils.isEmpty(taskName));

                symbolBgn = "@";
                Assert.assertTrue(x.contains(symbolBgn));

                indexBgn = x.indexOf(symbolBgn);
                Assert.assertTrue(indexBgn < x.length() - 1);

                String hexString = x.substring(indexBgn + symbolBgn.length());
                int hashcode = Integer.parseInt(hexString, 16);
                Assert.assertTrue(hashcode > 0);

                Assert.assertEquals(taskName, computeTaskName(x));
                Assert.assertEquals(hashcode, computeHashcode(x));
            }

            private String computeTaskName(String rawInput) {
                if (TextUtils.isEmpty(rawInput)) return null;
                String symbolBgn = "} ";
                String symbolEnd = "@";
                int indexBgn = rawInput.indexOf(symbolBgn);
                int indexEnd = rawInput.lastIndexOf(symbolEnd);
                if (indexBgn >= indexEnd - 1) return null;
                return rawInput.substring(indexBgn + symbolBgn.length(), indexEnd);
            }

            private int computeHashcode(String rawInput) {
                if (TextUtils.isEmpty(rawInput)) return -1;
                String symbolBgn = "@";
                String symbolEnd = ": ";
                int indexBgn = rawInput.indexOf(symbolBgn);
                int indexEnd = rawInput.contains(symbolEnd) ? rawInput.lastIndexOf(symbolEnd) : Integer.MAX_VALUE;
                if (indexBgn >= indexEnd - 1) return -1;
                String hexString = indexEnd == Integer.MAX_VALUE ? rawInput.substring(indexBgn + symbolBgn.length()) : rawInput.substring(indexBgn + symbolBgn.length(), indexEnd);
                try {
                    return Integer.parseInt(hexString, 16);
                } catch (NumberFormatException ignored) {
                    return -1;
                }
            }
        });

        synchronized (hasStart) {
            if (hasStart.get()) {
                Assert.assertTrue(hasFinish.get());
            } else {
                Assert.assertFalse(hasFinish.get());
            }
        }

        Handler handler = new Handler(handlerThread.getLooper());
        handler.post(new TestTask());

        synchronized (hasFinish) {
            hasFinish.wait();
        }

        Assert.assertTrue(hasStart.get());
        Assert.assertTrue(hasFinish.get());
    }


    private BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(LocationMonitorFeature.class)
                .enableBuiltinForegroundNotify(false)
                .enableForegroundMode(false)
                .wakelockTimeout(1000)
                .greyJiffiesTime(100)
                .foregroundLoopCheckTime(1000)
                .build();
        return new BatteryMonitorCore(config);
    }

    @Test
    public void testWatchHandlerThread() throws InterruptedException {
        final AtomicBoolean hasStart = new AtomicBoolean();
        final AtomicBoolean hasCheck = new AtomicBoolean();

        final LooperTaskMonitorFeature feature = new LooperTaskMonitorFeature();
        BatteryMonitorCore core = mockMonitor();
        core.start();
        feature.configure(core);
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(core.getConfig());
        Matrix.with().getPlugins().add(plugin);
        feature.onTurnOn();

        HandlerThread handlerThread = new HandlerThread("looper-test");
        handlerThread.start();

        Assert.assertTrue(feature.mWatchingList.isEmpty());
        Assert.assertTrue(feature.mLooperMonitorTrace.isEmpty());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        feature.watchLooper(handlerThread);
        Assert.assertEquals(1, feature.mWatchingList.size());
        Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        Handler handler = new Handler(handlerThread.getLooper());
        handler.post(new TestTask() {
            @Override
            public void run() {
                Assert.assertEquals(1, feature.mWatchingList.size());
                Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
                Assert.assertEquals(1, feature.mTaskJiffiesTrace.size());

                ArrayList<TimeBreaker.Stamp> taskStamps = feature.getTaskStamps(Process.myTid());
                Assert.assertNotNull(taskStamps);
                Assert.assertTrue(taskStamps.size() > 1);
                Assert.assertTrue(taskStamps.get(0).key.contains(MonitorFeatureLooperTest.class.getName() + "$"));

                hasStart.set(true);
                while (!hasCheck.get()) {}
            }
        });

        while (!hasStart.get()) {}

        Assert.assertEquals(1, feature.mWatchingList.size());
        Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
        Assert.assertEquals(1, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        hasCheck.set(true);
        while (feature.mDeltaList.isEmpty()) {}

        Assert.assertEquals(1, feature.mDeltaList.size());
        Assert.assertTrue(feature.mDeltaList.get(0).dlt.name.contains(MonitorFeatureLooperTest.class.getName() + "$"));

        List<Delta<TaskJiffiesSnapshot>> deltas = feature.currentJiffies();
        Assert.assertEquals(1, deltas.size());
        Assert.assertEquals(feature.mDeltaList, deltas);

        feature.clearFinishedJiffies();
        Assert.assertEquals(1, feature.mWatchingList.size());
        Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        feature.mDeltaList.addAll(deltas);

        feature.onTurnOff();
        Assert.assertTrue(feature.mWatchingList.isEmpty());
        Assert.assertTrue(feature.mLooperMonitorTrace.isEmpty());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());
    }

    @Test
    public void testWatchMainThread() throws InterruptedException {
        final AtomicBoolean hasStart = new AtomicBoolean();
        final AtomicBoolean hasCheck = new AtomicBoolean();

        final LooperTaskMonitorFeature feature = new LooperTaskMonitorFeature();
        BatteryMonitorCore core = mockMonitor();
        core.start();
        feature.configure(core);
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(core.getConfig());
        Matrix.with().getPlugins().add(plugin);
        feature.onTurnOn();


        Assert.assertTrue(feature.mWatchingList.isEmpty());
        Assert.assertTrue(feature.mLooperMonitorTrace.isEmpty());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        feature.watchLooper("main", Looper.getMainLooper());
        Assert.assertEquals(1, feature.mWatchingList.size());
        Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new TestTask() {
            @Override
            public void run() {
                Assert.assertEquals(1, feature.mWatchingList.size());
                Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
                Assert.assertEquals(1, feature.mTaskJiffiesTrace.size());

                ArrayList<TimeBreaker.Stamp> taskStamps = feature.getTaskStamps(Process.myTid());
                Assert.assertNotNull(taskStamps);
                Assert.assertTrue(taskStamps.size() > 1);
                Assert.assertTrue(taskStamps.get(0).key.contains(MonitorFeatureLooperTest.class.getName() + "$"));

                hasStart.set(true);
                while (!hasCheck.get()) {}
            }
        });

        while (!hasStart.get()) {}

        Assert.assertEquals(1, feature.mWatchingList.size());
        Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
        Assert.assertEquals(1, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        hasCheck.set(true);
        while (feature.mDeltaList.isEmpty()) {}

        Assert.assertEquals(1, feature.mDeltaList.size());
        Assert.assertTrue(feature.mDeltaList.get(0).dlt.name.contains(MonitorFeatureLooperTest.class.getName() + "$"));

        List<Delta<TaskJiffiesSnapshot>> deltas = feature.currentJiffies();
        Assert.assertEquals(1, deltas.size());
        Assert.assertEquals(feature.mDeltaList, deltas);

        feature.clearFinishedJiffies();
        Assert.assertEquals(1, feature.mWatchingList.size());
        Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        feature.mDeltaList.addAll(deltas);

        feature.onTurnOff();
        Assert.assertTrue(feature.mWatchingList.isEmpty());
        Assert.assertTrue(feature.mLooperMonitorTrace.isEmpty());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());
    }

    @Test
    public void testHandlerMessageTrace() throws InterruptedException {
        final AtomicBoolean hasStart = new AtomicBoolean();
        final AtomicBoolean hasCheck = new AtomicBoolean();

        final LooperTaskMonitorFeature feature = new LooperTaskMonitorFeature();
        BatteryMonitorCore core = mockMonitor();
        core.start();
        feature.configure(core);
        BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(core.getConfig());
        Matrix.with().getPlugins().add(plugin);
        feature.onTurnOn();


        Assert.assertTrue(feature.mWatchingList.isEmpty());
        Assert.assertTrue(feature.mLooperMonitorTrace.isEmpty());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        feature.watchLooper("main", Looper.getMainLooper());
        Assert.assertEquals(1, feature.mWatchingList.size());
        Assert.assertEquals(1, feature.mLooperMonitorTrace.size());
        Assert.assertEquals(0, feature.mTaskJiffiesTrace.size());
        Assert.assertTrue(feature.mDeltaList.isEmpty());

        final Handler handler = new Handler(Looper.getMainLooper());
        core.getHandler().post(new Runnable() {
            @Override
            public void run() {
                handler.sendEmptyMessage(22);
                Message message = Message.obtain();
                message.what = 33;
                handler.sendMessage(message);
                handler.post(new Runnable() {
                    @Override
                    public void run() {

                    }
                });
                hasStart.set(true);
                while (!hasCheck.get()) {}
            }
        });

        Assert.assertTrue(feature.mDeltaList.isEmpty());

        hasCheck.set(true);
        while (feature.mDeltaList.isEmpty()) {}
    }

    @Test
    public void testTraceUiThread() throws InterruptedException {
        AtomicReference<Integer> integer = new AtomicReference(new Integer(0));
        AtomicReference<Integer> ref = new AtomicReference();

        final CountDownLatch latch = new CountDownLatch(1);
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                latch.countDown();
            }
        });
        latch.await();
    }


    private static class TestTask implements Runnable {
        @Override
        public void run() {
        }
    }
}
