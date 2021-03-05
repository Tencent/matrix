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
import android.os.Handler;
import android.os.HandlerThread;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.text.TextUtils;

import com.tencent.matrix.trace.core.LooperMonitor;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.atomic.AtomicBoolean;


@RunWith(AndroidJUnit4.class)
public class LooperMonitorTest {
    static final String TAG = "Matrix.test.LooperMonitorTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
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

    private static class TestTask implements Runnable {
        @Override
        public void run() {
        }
    }
}
