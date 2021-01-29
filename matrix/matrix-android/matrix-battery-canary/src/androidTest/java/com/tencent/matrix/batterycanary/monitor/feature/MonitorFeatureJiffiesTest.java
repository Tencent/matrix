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
import android.arch.core.util.Function;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;


@RunWith(AndroidJUnit4.class)
public class MonitorFeatureJiffiesTest {
    static final String TAG = "Matrix.test.MonitorFeatureJiffiesTest";

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

    @Test
    public void getGetProcInfo() {
        JiffiesMonitorFeature.ProcessInfo processInfo = JiffiesMonitorFeature.ProcessInfo.getProcessInfo();
        Assert.assertNotNull(processInfo);
        Assert.assertEquals(mContext.getPackageName(), processInfo.name);
        Assert.assertTrue(processInfo.threadInfo.size() > 0);

        for (JiffiesMonitorFeature.ProcessInfo.ThreadInfo item : processInfo.threadInfo) {
            Assert.assertTrue(item.tid > 0);
        }
    }

    @Test
    public void testDump() {
        JiffiesSnapshot snapshot = JiffiesSnapshot.currentJiffiesSnapshot(JiffiesMonitorFeature.ProcessInfo.getProcessInfo(), false);
        Assert.assertNotNull(snapshot);
        Assert.assertEquals(Process.myPid(), snapshot.pid);
        Assert.assertEquals(mContext.getPackageName(), snapshot.name);
        Assert.assertFalse(snapshot.threadEntries.getList().isEmpty());
        new BatteryMonitorCallback.BatteryPrinter().onWatchingThreads(snapshot.threadEntries);
    }

    @Test
    public void getGetJiffiesSnapshot() {
        JiffiesSnapshot snapshot = JiffiesSnapshot.currentJiffiesSnapshot(JiffiesMonitorFeature.ProcessInfo.getProcessInfo(), false);
        Assert.assertNotNull(snapshot);
        Assert.assertEquals(Process.myPid(), snapshot.pid);
        Assert.assertEquals(mContext.getPackageName(), snapshot.name);
        Assert.assertFalse(snapshot.threadEntries.getList().isEmpty());
        long total = 0;
        for (JiffiesSnapshot.ThreadJiffiesSnapshot item : snapshot.threadEntries.getList()) {
            total += item.value;
        }
        Assert.assertEquals(total, snapshot.totalJiffies.get().longValue());
    }

    @Test
    public void testJiffiesDiff() {
        final JiffiesSnapshot bgn = JiffiesSnapshot.currentJiffiesSnapshot(JiffiesMonitorFeature.ProcessInfo.getProcessInfo(), false);
        final JiffiesSnapshot end = JiffiesSnapshot.currentJiffiesSnapshot(JiffiesMonitorFeature.ProcessInfo.getProcessInfo(), false);
        Delta<JiffiesSnapshot> delta = end.diff(bgn);

        Assert.assertNotNull(delta);
        Assert.assertSame(bgn, delta.bgn);
        Assert.assertSame(end, delta.end);
        Assert.assertEquals(delta.dlt.totalJiffies.value.longValue(), delta.end.totalJiffies.value - delta.bgn.totalJiffies.value);

        Function<Integer, JiffiesSnapshot.ThreadJiffiesSnapshot> findLastOne = new Function<Integer, JiffiesSnapshot.ThreadJiffiesSnapshot>() {
            @Override
            public JiffiesSnapshot.ThreadJiffiesSnapshot apply(Integer tid) {
                for (JiffiesSnapshot.ThreadJiffiesSnapshot item : bgn.threadEntries.getList()) {
                    if (item.tid == tid) return item;
                }
                return null;
            }
        };
        Function<Integer, JiffiesSnapshot.ThreadJiffiesSnapshot> findCurrOne = new Function<Integer, JiffiesSnapshot.ThreadJiffiesSnapshot>() {
            @Override
            public JiffiesSnapshot.ThreadJiffiesSnapshot apply(Integer tid) {
                for (JiffiesSnapshot.ThreadJiffiesSnapshot item : end.threadEntries.getList()) {
                    if (item.tid == tid) return item;
                }
                return null;
            }
        };

        for (JiffiesSnapshot.ThreadJiffiesSnapshot diff : delta.dlt.threadEntries.getList()) {
            JiffiesSnapshot.ThreadJiffiesSnapshot last = findLastOne.apply(diff.tid);
            JiffiesSnapshot.ThreadJiffiesSnapshot curr = findCurrOne.apply(diff.tid);
            Assert.assertNotNull(curr);

            if (diff.isNewAdded) {
                Assert.assertNull(last);
                Assert.assertEquals(diff.value.longValue(), curr.value.longValue());
            } else {
                Assert.assertNotNull(last);
                Assert.assertEquals(diff.value.longValue(), curr.value - last.value);
            }
        }
    }

    @Test
    public void testJiffiesDiffWithNewThread() {
        final JiffiesSnapshot bgn = JiffiesSnapshot.currentJiffiesSnapshot(JiffiesMonitorFeature.ProcessInfo.getProcessInfo(), false);
        final JiffiesSnapshot end = JiffiesSnapshot.currentJiffiesSnapshot(JiffiesMonitorFeature.ProcessInfo.getProcessInfo(), false);

        long jiffies = 10016L;
        final JiffiesSnapshot.ThreadJiffiesSnapshot mockThreadJiffies = new JiffiesSnapshot.ThreadJiffiesSnapshot(jiffies);
        mockThreadJiffies.name = "mock-thread";
        mockThreadJiffies.tid = 10086;
        mockThreadJiffies.isNewAdded = true;
        end.threadEntries.getList().add(mockThreadJiffies);
        end.totalJiffies.value += jiffies;

        Delta<JiffiesSnapshot> delta = end.diff(bgn);
        Assert.assertNotNull(delta);
        Assert.assertSame(bgn, delta.bgn);
        Assert.assertSame(end, delta.end);
        Assert.assertTrue(delta.dlt.threadEntries.getList().size() > 0);
        Assert.assertEquals(delta.dlt.totalJiffies.value.longValue(), delta.end.totalJiffies.value - delta.bgn.totalJiffies.value);

        Function<List<JiffiesSnapshot.ThreadJiffiesSnapshot>, JiffiesSnapshot.ThreadJiffiesSnapshot> findNewThread = new Function<List<JiffiesSnapshot.ThreadJiffiesSnapshot>, JiffiesSnapshot.ThreadJiffiesSnapshot>() {
            @Override
            public JiffiesSnapshot.ThreadJiffiesSnapshot apply(List<JiffiesSnapshot.ThreadJiffiesSnapshot> list) {
                for (JiffiesSnapshot.ThreadJiffiesSnapshot item : list) {
                    if (mockThreadJiffies.name.equals(item.name) && item.tid == mockThreadJiffies.tid ) return item;
                }
                return null;
            }
        };

        JiffiesSnapshot.ThreadJiffiesSnapshot last = findNewThread.apply(delta.bgn.threadEntries.getList());
        JiffiesSnapshot.ThreadJiffiesSnapshot curr = findNewThread.apply(delta.end.threadEntries.getList());
        JiffiesSnapshot.ThreadJiffiesSnapshot diff = findNewThread.apply(delta.dlt.threadEntries.getList());
        Assert.assertNull(last);
        Assert.assertNotNull(curr);
        Assert.assertNotNull(diff);
        Assert.assertTrue(diff.isNewAdded);
        Assert.assertEquals(diff.value.longValue(), curr.value.longValue());
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
    public void testJiffiesForegroundLoopCheck() throws InterruptedException {
        if (TestUtils.isAssembleTest()) { return; }

        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                    new Handler(Looper.getMainLooper());
                }
            }
        });
        thread.setPriority(Thread.MAX_PRIORITY);
        thread.setName("test-jiffies-thread");
        thread.start();

        final JiffiesMonitorFeature feature = new JiffiesMonitorFeature();
        final BatteryMonitorCore monitor = mockMonitor();
        monitor.enableForegroundLoopCheck(true);
        feature.configure(monitor);
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                monitor.onForeground(true);
            }
        });
        Thread.sleep(500000L);
    }

    @Test
    public void testJiffiesBackgroundCheck() throws InterruptedException {
        if (TestUtils.isAssembleTest()) { return; }

        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                    new Handler(Looper.getMainLooper());
                }
            }
        });
        thread.setPriority(Thread.MAX_PRIORITY);
        thread.setName("test-jiffies-thread");
        thread.start();

        JiffiesMonitorFeature feature = new JiffiesMonitorFeature();
        feature.configure(mockMonitor());
        mockForegroundSwitching(feature);
        Thread.sleep(10000L);
    }

    private void mockForegroundSwitching(final MonitorFeature core) {
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                core.onForeground(false);
            }
        });
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                core.onForeground(true);
            }
        }, 2000L);
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                core.onForeground(false);
            }
        }, 5000L);
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                core.onForeground(true);
            }
        }, 9000L);
    }
}
