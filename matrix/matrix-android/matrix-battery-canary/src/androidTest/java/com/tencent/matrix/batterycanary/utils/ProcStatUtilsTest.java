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

package com.tencent.matrix.batterycanary.utils;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Process;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.util.MatrixLog;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@SuppressWarnings("SpellCheckingInspection")
@RunWith(AndroidJUnit4.class)
public class ProcStatUtilsTest {
    static final String TAG = "Matrix.test.ProcessCpuTrackUtilsTest";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testGetCurrentMethodName() {
        Assert.assertEquals(
                "testGetCurrentMethodName",
                new Object() {
                }
                        .getClass()
                        .getEnclosingMethod()
                        .getName()
        );

        StackTraceElement[] ste = Thread.currentThread().getStackTrace();
        Assert.assertEquals("testGetCurrentMethodName", ste[2].getMethodName());
    }

    @Test
    public void testGetThreadJiffies() {
        ProcStatUtil.ProcStat stat = ProcStatUtil.current();
        Assert.assertNotNull(stat);
        Assert.assertNotNull(stat.comm);
        Assert.assertTrue(stat.utime >= 0);
        Assert.assertTrue(stat.stime >= 0);
        Assert.assertTrue(stat.cutime >= 0);
        Assert.assertTrue(stat.cstime >= 0);
        Assert.assertTrue(stat.getJiffies() >= 0);

        ProcStatUtil.ProcStat end = ProcStatUtil.current();
        Assert.assertNotNull(end);
        Assert.assertEquals(stat.comm, end.comm);
        Assert.assertTrue(end.getJiffies() - stat.getJiffies() >= 0);
    }

    @Test
    public void testGetThreadJiffiesDelta() {
        String message = "";
        long bgnMillis, endMillis;
        ProcStatUtil.ProcStat bgn, end;

        for (int round = 0; round < 10; round++) {
            message += "\nROUND " + round;

            bgnMillis = SystemClock.currentThreadTimeMillis();
            bgn = ProcStatUtil.current();
            Assert.assertNotNull(bgn);

            for (int i = 0; i < 10000; i++) {
                Assert.assertNotNull(ProcStatUtil.current());
            }

            endMillis = SystemClock.currentThreadTimeMillis();
            end = ProcStatUtil.current();
            Assert.assertNotNull(end);

            message += "\nbgn: " + (bgnMillis) + " vs " + (bgn.getJiffies());
            message += "\nend: " + (endMillis) + " vs " + (end.getJiffies());
            message += "\ndlt:" + (endMillis - bgnMillis) + " vs " + (end.getJiffies() - bgn.getJiffies());
        }

        if (!TestUtils.isAssembleTest()) {
            Assert.fail(message);
        }
    }

    /**
     * cat: /proc/loadavg
     */
    @Test
    public void testGetCpuLoad() {
        String cat = BatteryCanaryUtil.cat("/proc/loadavg");
        Assert.assertTrue(TextUtils.isEmpty(cat));
    }

    /**
     * cat: /proc/stat
     */
    @Test
    public void testGetProcStat() {
        String cat = BatteryCanaryUtil.cat("/proc/stat");
        Assert.assertTrue(TextUtils.isEmpty(cat));
    }

    /**
     * cat: /proc/<mypid>/stat
     * example:
     * 11159 (terycanary.test) S 699 699 0 0 -1 1077952832 11204 0 0 0 39 8 0 0 20 0 20 0 9092893 5475467264 22598 18446744073709551615 421814448128 421814472944 549131058960 0 0 0 4612 1 1073775864 0 0 0 17 5 0 0 0 0 0 421814476800 421814478232 422247952384 549131060923 549131061022 549131061022 549131063262 0
     */
    @Test
    public void testGetMyProcStat() {
        String catPath = "/proc/" + Process.myPid() + "/stat";
        String cat = BatteryCanaryUtil.cat(catPath);
        Assert.assertFalse(TextUtils.isEmpty(cat));

        ProcStatUtil.ProcStat stat = parseJiffiesInfoWithSplitsForPath(catPath);
        Assert.assertNotNull(stat.comm);
        Assert.assertTrue(stat.utime >= 0);
        Assert.assertTrue(stat.stime >= 0);
        Assert.assertTrue(stat.cutime >= 0);
        Assert.assertTrue(stat.cstime >= 0);
        long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertTrue(jiffies >= 0);

        ProcStatUtil.ProcStat statNew = ProcStatUtil.currentPid();
        Assert.assertNotNull(statNew);
        Assert.assertEquals(stat.comm, statNew.comm);
        Assert.assertTrue(statNew.utime  >= stat.utime );
        Assert.assertTrue(statNew.stime  >= stat.stime );
        Assert.assertTrue(statNew.cutime >= stat.cutime);
        Assert.assertTrue(statNew.cstime >= stat.cstime);
        Assert.assertTrue(statNew.getJiffies() >= stat.getJiffies());
    }

    /**
     * cat: /proc/<mypid>/task/<tid>/stat
     * example:
     * 10966 (terycanary.test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 0 0 20 0 17 0 9087400 5414273024 24109 18446744073709551615 421814448128 421814472944 549131058960 0 0 0 4612 1 1073775864 1 0 0 17 7 0 0 0 0 0 421814476800 421814478232 422247952384 549131060923 549131061022 549131061022 549131063262 0
     */
    @Test
    public void testGetMyProcThreadStat() {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));

                ProcStatUtil.ProcStat stat = parseJiffiesInfoWithSplitsForPath(catPath);
                Assert.assertNotNull(stat.comm);
                Assert.assertTrue(stat.utime >= 0);
                Assert.assertTrue(stat.stime >= 0);
                Assert.assertTrue(stat.cutime >= 0);
                Assert.assertTrue(stat.cstime >= 0);
                long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
                Assert.assertTrue(jiffies >= 0);
            }
        }
    }


    @Test
    public void testGetMyProcStatAndTProcThreadStat() {
        ProcStatUtil.ProcStat procStat = parseJiffiesInfoWithBufferForPathR2("/proc/" + Process.myPid() + "/stat");
        Assert.assertNotNull(procStat);

        ProcStatUtil.ProcStat accStat = new ProcStatUtil.ProcStat();
        String dirPath = "/proc/" + Process.myPid() + "/task";
        File[] tasksFile = new File(dirPath).listFiles();
        for (File item : tasksFile) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBufferForPathR2(catPath);
                Assert.assertNotNull(stat);
                accStat.utime += stat.utime;
                accStat.stime += stat.stime;
                accStat.cutime += stat.cutime;
                accStat.cstime += stat.cstime;
            }
        }

        if (!TestUtils.isAssembleTest()) {
            String msg = "";
            msg += "\n Thread count: " + Thread.getAllStackTraces().size() + " vs " + tasksFile.length;
            msg += "\n jiffies utime: " + procStat.utime + " vs " + accStat.utime;
            msg += "\n jiffies stime: " + procStat.stime + " vs " + accStat.stime;
            msg += "\n jiffies cutime: " + procStat.cutime + " vs " + accStat.cutime;
            msg += "\n jiffies cstime: " + procStat.cstime + " vs " + accStat.cstime;
            msg += "\n jiffies total: " + procStat.getJiffies() + " vs " + accStat.getJiffies();
            Assert.fail(msg);
        }
    }

    @Test
    public void testProcStatShouldAlwaysInc() throws InterruptedException {
        final ProcStatUtil.ProcStat procStat = parseJiffiesInfoWithBufferForPathR2("/proc/" + Process.myPid() + "/stat");
        Assert.assertNotNull(procStat);

        List<Thread> threadList = new LinkedList<>();
        for (int i = 0; i < 100; i++) {
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    for (int j = 0; j < Integer.MAX_VALUE; j++);
                    ProcStatUtil.ProcStat curr = parseJiffiesInfoWithBufferForPathR2("/proc/" + Process.myPid() + "/stat");
                    Assert.assertNotNull(curr);
                    Assert.assertTrue(curr.getJiffies() >= procStat.getJiffies());
                }
            });
            thread.start();
            threadList.add(thread);
        }

        for (Thread item : threadList) {
            item.join();
            while (item.isAlive());
            ProcStatUtil.ProcStat curr = parseJiffiesInfoWithBufferForPathR2("/proc/" + Process.myPid() + "/stat");
            Assert.assertNotNull(curr);
            Assert.assertTrue(curr.getJiffies() > procStat.getJiffies());
        }

        ProcStatUtil.ProcStat curr = parseJiffiesInfoWithBufferForPathR2("/proc/" + Process.myPid() + "/stat");
        Assert.assertNotNull(curr);
        Assert.assertTrue(curr.getJiffies() > procStat.getJiffies());
        Assert.assertTrue(Thread.getAllStackTraces().size() < 100);
    }

    @SuppressWarnings("ConstantConditions")
    @Test
    public void testGetMyProcThreadStatOpt() {
        String dirPath = "/proc/" + Process.myPid() + "/task";

        File[] tasksFile = new File(dirPath).listFiles();
        for (File item : tasksFile) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));

                ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBufferForPath(catPath, new byte[2 * 1024]);
                Assert.assertNotNull(stat.comm);
                Assert.assertTrue(stat.utime >= 0);
                Assert.assertTrue(stat.stime >= 0);
                Assert.assertTrue(stat.cutime >= 0);
                Assert.assertTrue(stat.cstime >= 0);
                long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
                Assert.assertTrue(jiffies >= 0);
            }
        }
    }

    @SuppressWarnings("ConstantConditions")
    @Test
    public void testGetMyProcThreadStatOptR2() {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));

                ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBufferForPathR2(catPath);
                Assert.assertNotNull(stat.comm);
                Assert.assertTrue(stat.utime >= 0);
                Assert.assertTrue(stat.stime >= 0);
                Assert.assertTrue(stat.cutime >= 0);
                Assert.assertTrue(stat.cstime >= 0);
                long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
                Assert.assertTrue(jiffies >= 0);
            }
        }
    }

    @SuppressWarnings("ConstantConditions")
    @Test
    public void testGetMyProcThreadStatAndCompare() {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));
                ProcStatUtil.ProcStat statInfo1 = ProcStatUtil.parseWithSplits(cat);
                ProcStatUtil.ProcStat statInfo2 = parseJiffiesInfoWithBuffer(cat.getBytes());
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);

                cat = getProStatText(catPath);
                statInfo1 = ProcStatUtil.parseWithSplits(cat);
                statInfo2 = parseJiffiesInfoWithBuffer(cat.getBytes());
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);
            }
        }
    }

    @Test
    public void testReadBuffer() {
        String sample = "10966 (terycanary.test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 33 3";
        Assert.assertEquals(72, sample.length());
        Assert.assertEquals(72, sample.getBytes().length);

        ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBuffer(sample.getBytes());
        Assert.assertEquals("terycanary.test", stat.comm);
        Assert.assertEquals(22, stat.utime);
        Assert.assertEquals(2, stat.stime);
        Assert.assertEquals(33, stat.cutime);
        Assert.assertEquals(3, stat.cstime);
        long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertEquals(22 + 2 + 33 + 3, jiffies);

        sample = "10966 (terycanary test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 33 3";
        stat = parseJiffiesInfoWithBuffer(sample.getBytes());
        Assert.assertEquals("terycanary test", stat.comm);
        Assert.assertEquals(22, stat.utime);
        Assert.assertEquals(2, stat.stime);
        Assert.assertEquals(33, stat.cutime);
        Assert.assertEquals(3, stat.cstime);
        jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertEquals(22 + 2 + 33 + 3, jiffies);

        sample = "10966 (tery canary test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 33 3";
        stat = parseJiffiesInfoWithBuffer(sample.getBytes());
        Assert.assertEquals("tery canary test", stat.comm);
        Assert.assertEquals(22, stat.utime);
        Assert.assertEquals(2, stat.stime);
        Assert.assertEquals(33, stat.cutime);
        Assert.assertEquals(3, stat.cstime);
        jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertEquals(22 + 2 + 33 + 3, jiffies);
    }

    @SuppressWarnings("ConstantConditions")
    @Test
    public void testGetMyProcThreadStatAndCompare2() {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));
                ProcStatUtil.ProcStat statInfo1 = parseJiffiesInfoWithBuffer(cat.getBytes());
                ProcStatUtil.ProcStat statInfo2 = parseJiffiesInfoWithBuffer(Arrays.copyOfRange(cat.getBytes(), 0, 127));
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);
            }
        }
    }

    static ProcStatUtil.ProcStat parseJiffiesInfoWithSplitsForPath(String path) {
        return ProcStatUtil.parseWithSplits(BatteryCanaryUtil.cat(path));
    }

    static ProcStatUtil.ProcStat parseJiffiesInfoWithBufferForPath(String path, byte[] buffer) {
        File file = new File(path);
        if (!file.exists()) {
            return null;
        }

        int readBytes;
        try (FileInputStream fis = new FileInputStream(file)) {
            readBytes = fis.read(buffer);
        } catch (IOException e) {
            MatrixLog.printErrStackTrace(TAG, e, "read buffer from file fail");
            readBytes = -1;
        }
        if (readBytes <= 0) {
            return null;
        }

        return parseJiffiesInfoWithBuffer(buffer);
    }

    static ProcStatUtil.ProcStat parseJiffiesInfoWithBufferForPathR2(String path) {
        String text = getProStatText(path);
        if (TextUtils.isEmpty(text)) return null;
        //noinspection ConstantConditions
        return parseJiffiesInfoWithBuffer(text.getBytes());
    }

    static String getProStatText(String path) {
        File file = new File(path);
        if (!file.exists()) {
            return null;
        }

        StringBuilder sb = new StringBuilder();
        int spaceCount = 0;
        int readBytes;
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
            while ((readBytes = reader.read()) != -1) {
                char character = (char) readBytes;
                sb.append(character);
                if (' ' == character) {
                    spaceCount++;
                    if (spaceCount > 20) {
                        break;
                    }
                }
            }
        } catch (IOException e) {
            MatrixLog.printErrStackTrace(TAG, e, "read buffer from file fail");
            readBytes = -1;
        }

        if (readBytes <= 0) {
            return null;
        }

        return sb.toString();
    }

    static ProcStatUtil.ProcStat parseJiffiesInfoWithBuffer(byte[] statBuffer) {
        ProcStatUtil.ProcStat stat = ProcStatUtil.parseWithBuffer(statBuffer);

        Assert.assertNotNull(stat.comm);
        Assert.assertTrue(stat.utime >= 0);
        Assert.assertTrue(stat.stime >= 0);
        Assert.assertTrue(stat.cutime >= 0);
        Assert.assertTrue(stat.cstime >= 0);

        long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertTrue(jiffies >= 0);
        return stat;
    }


    @RunWith(AndroidJUnit4.class)
    public static class Benchmark {
        static final String TAG = "Matrix.test.ProcessCpuTrackUtilsTest$Benchmark";

        Context mContext;

        @Before
        public void setUp() {
            mContext = InstrumentationRegistry.getTargetContext();
        }

        @After
        public void shutDown() {
        }

        @Test
        public void testProcStatBenchmark() {
            if (TestUtils.isAssembleTest()) return;
            long delta1, delta2;
            long bgnMillis, endMillis;

            bgnMillis = SystemClock.currentThreadTimeMillis();
            for (int i = 0; i < 10000; i++) {
                SystemClock.currentThreadTimeMillis();
            }
            endMillis = SystemClock.currentThreadTimeMillis();
            delta1 = endMillis - bgnMillis;

            bgnMillis = SystemClock.currentThreadTimeMillis();
            for (int i = 0; i < 10000; i++) {
                ProcStatUtil.current();
            }
            endMillis = SystemClock.currentThreadTimeMillis();
            delta2 = endMillis - bgnMillis;


            Assert.fail("TIME CONSUMED: " + delta1 + " vs " + delta2);
        }

        @Test
        public void testGetMyProcThreadStatBenchmark() {
            if (TestUtils.isAssembleTest()) return;

            int times = 100;
            long current = SystemClock.uptimeMillis();
            for (int i = 0; i < times; i++) {
                String dirPath = "/proc/" + Process.myPid() + "/task";
                for (File item : new File(dirPath).listFiles()) {
                    if (item.isDirectory()) {
                        String catPath = new File(item, "stat").getAbsolutePath();
                        parseJiffiesInfoWithSplitsForPath(catPath);
                    }
                }
            }
            long timeConsumed1 = SystemClock.uptimeMillis() - current;

            current = SystemClock.uptimeMillis();
            for (int i = 0; i < times; i++) {
                String dirPath = "/proc/" + Process.myPid() + "/task";
                for (File item : new File(dirPath).listFiles()) {
                    if (item.isDirectory()) {
                        String catPath = new File(item, "stat").getAbsolutePath();
                        parseJiffiesInfoWithBufferForPath(catPath, new byte[2 * 1024]);
                    }
                }
            }
            long timeConsumed2 = SystemClock.uptimeMillis() - current;

            current = SystemClock.uptimeMillis();
            for (int i = 0; i < times; i++) {
                String dirPath = "/proc/" + Process.myPid() + "/task";
                for (File item : new File(dirPath).listFiles()) {
                    if (item.isDirectory()) {
                        String catPath = new File(item, "stat").getAbsolutePath();
                        parseJiffiesInfoWithBufferForPathR2(catPath);
                    }
                }
            }
            long timeConsumed3 = SystemClock.uptimeMillis() - current;

            Assert.fail("TIME CONSUMED: " + timeConsumed1 + " vs " + timeConsumed2 + " vs " + timeConsumed3);
        }

        @Test
        public void testGetMyProcThreadStatWithBufferBenchmark() {
            if (TestUtils.isAssembleTest()) return;

            int times = 100;
            long current = SystemClock.uptimeMillis();
            for (int i = 0; i < times; i++) {
                String dirPath = "/proc/" + Process.myPid() + "/task";
                for (File item : new File(dirPath).listFiles()) {
                    if (item.isDirectory()) {
                        String catPath = new File(item, "stat").getAbsolutePath();
                        ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBufferForPath(catPath, new byte[2 * 1024]);
                        Assert.assertNotNull(stat.comm);
                        Assert.assertTrue(stat.utime >= 0);
                        Assert.assertTrue(stat.stime >= 0);
                        Assert.assertTrue(stat.cutime >= 0);
                        Assert.assertTrue(stat.cstime >= 0);
                        long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
                        Assert.assertTrue(jiffies >= 0);
                    }
                }
            }
            long timeConsumed1 = SystemClock.uptimeMillis() - current;

            current = SystemClock.uptimeMillis();
            for (int i = 0; i < times; i++) {
                String dirPath = "/proc/" + Process.myPid() + "/task";
                for (File item : new File(dirPath).listFiles()) {
                    if (item.isDirectory()) {
                        String catPath = new File(item, "stat").getAbsolutePath();
                        ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBufferForPath(catPath, new byte[128]);
                        Assert.assertNotNull(stat.comm);
                        Assert.assertTrue(stat.utime >= 0);
                        Assert.assertTrue(stat.stime >= 0);
                        Assert.assertTrue(stat.cutime >= 0);
                        Assert.assertTrue(stat.cstime >= 0);
                        long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
                        Assert.assertTrue(jiffies >= 0);
                    }
                }
            }
            long timeConsumed2 = SystemClock.uptimeMillis() - current;

            Assert.fail("TIME CONSUMED: " + timeConsumed1 + " vs " + timeConsumed2);
        }

    }

    @Test
    public void testSafeByteToString() {
        String foo = "ffffoooo";
        byte[] fooBytes = foo.getBytes(StandardCharsets.UTF_8);
        Assert.assertEquals(8, fooBytes.length);
        Assert.assertEquals(foo, new String(fooBytes, StandardCharsets.UTF_8));
        Assert.assertEquals(foo, ProcStatUtil.safeBytesToString(fooBytes, 0, fooBytes.length));
        Assert.assertEquals(new String(fooBytes, StandardCharsets.UTF_8), ProcStatUtil.safeBytesToString(fooBytes, 0, fooBytes.length));
        Assert.assertEquals("ffff", ProcStatUtil.safeBytesToString(fooBytes, 0, 4));
        Assert.assertEquals("oooo", ProcStatUtil.safeBytesToString(fooBytes, 4, 4));

        // Bounded
        Assert.assertEquals("", ProcStatUtil.safeBytesToString(fooBytes, -1, 4));
        Assert.assertEquals("", ProcStatUtil.safeBytesToString(fooBytes, 4, 5));
    }


    @RunWith(AndroidJUnit4.class)
    public static class MultiProcess {
        static final String TAG = "Matrix.test.ProcessCpuTrackUtilsTest$MultiProcess";

        Context mContext;

        @Before
        public void setUp() {
            mContext = InstrumentationRegistry.getTargetContext();
        }

        @After
        public void shutDown() {
        }

        @Test
        public void testListPids() {
            ActivityManager am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
            Assert.assertNotNull(am);
            List<ActivityManager.RunningAppProcessInfo> runningAppProcesses = am.getRunningAppProcesses();
            List<Integer> pids = new LinkedList<>();
            for (ActivityManager.RunningAppProcessInfo item : runningAppProcesses) {
                if (item.processName.startsWith(mContext.getPackageName())) {
                    pids.add(item.pid);
                }
            }
            Assert.assertEquals(1, pids.size());
            Assert.assertEquals(Process.myPid(), pids.get(0).intValue());
        }
    }
}
