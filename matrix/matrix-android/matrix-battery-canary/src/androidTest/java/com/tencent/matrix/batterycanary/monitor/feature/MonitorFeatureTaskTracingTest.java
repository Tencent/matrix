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
import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.utils.ProcStatUtil;
import com.tencent.matrix.util.MatrixLog;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureTaskTracingTest {
    static final String TAG = "Matrix.test.MonitorTaskStatTest";

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

    private static BatteryMonitorCore mockMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
                .enable(TestExecutorTaskMonitor.class)
                .build();
        return new BatteryMonitorCore(config);
    }

    @Test
    @Ignore
    public void testTaskTracing() {
    }


    @RunWith(AndroidJUnit4.class)
    public static final class Benchmark {
        static final String TAG = "Matrix.test.MonitorTaskStatTest.Benchmark";

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
        public void benchmarkWithSingleThread() throws InterruptedException {
            if (!TestUtils.isLaunchingFrom(Benchmark.class.getName())) {
                return;
            }

            final BatteryMonitorCore monitor = mockMonitor();
            BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
            Matrix.with().getPlugins().add(plugin);

            TestExecutorTaskMonitor feat = BatteryCanary.getMonitorFeature(TestExecutorTaskMonitor.class);

            Assert.assertNotNull(feat);

            int threadCount = 1;
            int loopCount = 100;

            long delta1, delta2;
            long bgnMillis, endMillis;
            feat.setThreadCount(threadCount);

            feat.setEnabled(false);
            final CountDownLatch latch1 = new CountDownLatch(loopCount);
            bgnMillis = System.currentTimeMillis();
            for (int i = 0; i < loopCount; i++) {
                final int finalI = i;
                feat.submit(new Runnable() {
                    @Override
                    public void run() {
                        MatrixLog.i(TAG, "execute task: " + finalI);
                        try {
                            Thread.sleep(100L);
                        } catch (InterruptedException e) {
                            throw new RuntimeException(e);
                        }
                        latch1.countDown();
                    }
                });
            }
            latch1.await();
            endMillis = System.currentTimeMillis();
            delta1 = endMillis - bgnMillis;
            Assert.assertEquals(0, feat.mTaskJiffiesTrace.size());


            feat.setEnabled(true);
            final CountDownLatch latch2 = new CountDownLatch(loopCount);
            bgnMillis = System.currentTimeMillis();
            for (int i = 0; i < loopCount; i++) {
                final int finalI = i;
                feat.submit(new Runnable() {
                    @Override
                    public void run() {
                        MatrixLog.i(TAG, "execute task: " + finalI);
                        try {
                            Thread.sleep(100L);
                        } catch (InterruptedException e) {
                            throw new RuntimeException(e);
                        }
                        latch2.countDown();
                    }
                });
            }
            latch2.await();
            endMillis = System.currentTimeMillis();
            delta2 = endMillis - bgnMillis;
            float incRatio = (delta2 - delta1) / ((float) delta1);
            Assert.assertEquals(0, feat.mTaskJiffiesTrace.size());
            Assert.assertTrue("TIME CONSUMED: " + delta1 + " vs " + delta2 + ", inc: " + incRatio, incRatio <= 0.02f);
        }

        @Test
        public void benchmarkWithLessThread() throws InterruptedException {
            if (!TestUtils.isLaunchingFrom(Benchmark.class.getName())) {
                return;
            }

            final BatteryMonitorCore monitor = mockMonitor();
            BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
            Matrix.with().getPlugins().add(plugin);
            TestExecutorTaskMonitor feat = BatteryCanary.getMonitorFeature(TestExecutorTaskMonitor.class);

            Assert.assertNotNull(feat);

            int threadCount = 5;
            int loopCount = 100;

            long delta1, delta2;
            long bgnMillis, endMillis;
            feat.setThreadCount(threadCount);

            feat.setEnabled(false);
            final CountDownLatch latch1 = new CountDownLatch(loopCount);
            bgnMillis = System.currentTimeMillis();
            for (int i = 0; i < loopCount; i++) {
                final int finalI = i;
                feat.submit(new Runnable() {
                    @Override
                    public void run() {
                        MatrixLog.i(TAG, "execute task: " + finalI);
                        try {
                            Thread.sleep(100L);
                        } catch (InterruptedException e) {
                            throw new RuntimeException(e);
                        }
                        latch1.countDown();
                    }
                });
            }
            latch1.await();
            endMillis = System.currentTimeMillis();
            delta1 = endMillis - bgnMillis;
            Assert.assertEquals(0, feat.mTaskJiffiesTrace.size());

            feat.setEnabled(true);
            final CountDownLatch latch2 = new CountDownLatch(loopCount);
            bgnMillis = System.currentTimeMillis();
            for (int i = 0; i < loopCount; i++) {
                final int finalI = i;
                feat.submit(new Runnable() {
                    @Override
                    public void run() {
                        MatrixLog.i(TAG, "execute task: " + finalI);
                        try {
                            Thread.sleep(100L);
                        } catch (InterruptedException e) {
                            throw new RuntimeException(e);
                        }
                        latch2.countDown();
                    }
                });
            }
            latch2.await();
            endMillis = System.currentTimeMillis();
            delta2 = endMillis - bgnMillis;
            float incRatio = (delta2 - delta1) / ((float) delta1);
            Assert.assertEquals(0, feat.mTaskJiffiesTrace.size());
            Assert.assertTrue("TIME CONSUMED: " + delta1 + " vs " + delta2 + ", inc: " + incRatio, incRatio <= 0.02f);
        }

        @Test
        public void benchmarkWithMoreThread() throws InterruptedException {
            if (!TestUtils.isLaunchingFrom(Benchmark.class.getName())) {
                return;
            }

            final BatteryMonitorCore monitor = mockMonitor();
            BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(monitor.getConfig());
            Matrix.with().getPlugins().add(plugin);
            TestExecutorTaskMonitor feat = BatteryCanary.getMonitorFeature(TestExecutorTaskMonitor.class);

            Assert.assertNotNull(feat);

            int threadCount = 10;
            int loopCount = 100;

            long delta1, delta2;
            long bgnMillis, endMillis;
            feat.setThreadCount(threadCount);

            feat.setEnabled(false);
            final CountDownLatch latch1 = new CountDownLatch(loopCount);
            bgnMillis = System.currentTimeMillis();
            for (int i = 0; i < loopCount; i++) {
                final int finalI = i;
                feat.submit(new Runnable() {
                    @Override
                    public void run() {
                        MatrixLog.i(TAG, "execute task: " + finalI);
                        try {
                            Thread.sleep(100L);
                        } catch (InterruptedException e) {
                            throw new RuntimeException(e);
                        }
                        latch1.countDown();
                    }
                });
            }
            latch1.await();
            endMillis = System.currentTimeMillis();
            delta1 = endMillis - bgnMillis;
            Assert.assertEquals(0, feat.mTaskJiffiesTrace.size());

            feat.setEnabled(true);
            final CountDownLatch latch2 = new CountDownLatch(loopCount);
            bgnMillis = System.currentTimeMillis();
            for (int i = 0; i < loopCount; i++) {
                final int finalI = i;
                feat.submit(new Runnable() {
                    @Override
                    public void run() {
                        MatrixLog.i(TAG, "execute task: " + finalI);
                        try {
                            Thread.sleep(100L);
                        } catch (InterruptedException e) {
                            throw new RuntimeException(e);
                        }
                        latch2.countDown();
                    }
                });
            }
            latch2.await();
            endMillis = System.currentTimeMillis();
            delta2 = endMillis - bgnMillis;
            float incRatio = (delta2 - delta1) / ((float) delta1);
            Assert.assertEquals(0, feat.mTaskJiffiesTrace.size());
            Assert.assertTrue("TIME CONSUMED: " + delta1 + " vs " + delta2 + ", inc: " + incRatio, incRatio <= 0.02f);
        }
    }


    public static final class TestExecutorTaskMonitor extends AbsTaskMonitorFeature {
        boolean enabled = false;
        int threadCount = 5;
        ExecutorService mExecutor;

        public TestExecutorTaskMonitor() {
            setThreadCount(threadCount);
        }

        public void setThreadCount(int threadCount) {
            this.threadCount = threadCount;
            mExecutor = Executors.newFixedThreadPool(threadCount);
        }

        @Override
        public int weight() {
            return 0;
        }

        public void setEnabled(boolean enabled) {
            this.enabled = enabled;
        }

        void submit(final Runnable task) {
            mExecutor.submit(new Callable<Void>() {
                @Override
                public Void call() throws Exception {
                    if (enabled) {
                        String key = task.getClass().getName();
                        int hashcode = task.hashCode();
                        onTaskStarted(key, hashcode);
                        task.run();
                        onTaskFinished(key, hashcode);
                    } else {
                        task.run();
                    }
                    return null;
                }
            });
        }
    }
}
