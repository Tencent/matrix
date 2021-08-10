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
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.util.MatrixLog;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.internal.util.io.IOUtil;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

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
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
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

    @Test
    public void testGetSleepThreadJiffiesDelta() throws InterruptedException {
        String message = "";
        long bgnMillis, endMillis;
        ProcStatUtil.ProcStat bgn, end;

        for (int round = 0; round < 10; round++) {
            message += "\nROUND " + round;

            bgnMillis = SystemClock.currentThreadTimeMillis();
            bgn = ProcStatUtil.current();
            Assert.assertNotNull(bgn);

            Thread.sleep(1000L);

            endMillis = SystemClock.currentThreadTimeMillis();
            end = ProcStatUtil.current();
            Assert.assertNotNull(end);

            message += "\nbgn: " + (bgnMillis) + " vs " + (bgn.getJiffies());
            message += "\nend: " + (endMillis) + " vs " + (end.getJiffies());
            message += "\ndlt:" + (endMillis - bgnMillis) + " vs " + (end.getJiffies() - bgn.getJiffies());
            message += "\n";
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
    public void testGetMyProcStat() throws ProcStatUtil.ParseException {
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
    public void testGetMyProcThreadStat() throws ProcStatUtil.ParseException {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));

                ProcStatUtil.ProcStat stat = parseJiffiesInfoWithSplitsForPath(catPath);
                Assert.assertNotNull(stat.comm);
                Assert.assertNotNull(stat.stat);
                Assert.assertTrue(Arrays.asList("R", "S").contains(stat.stat));
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
    public void testGetMyProcStatAndTProcThreadStat() throws ProcStatUtil.ParseException {
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
    public void testProcStatShouldAlwaysInc() throws InterruptedException, ProcStatUtil.ParseException {
        final ProcStatUtil.ProcStat procStat = parseJiffiesInfoWithBufferForPathR2("/proc/" + Process.myPid() + "/stat");
        Assert.assertNotNull(procStat);

        List<Thread> threadList = new LinkedList<>();
        for (int i = 0; i < 100; i++) {
            Thread thread = new Thread(new Runnable() {
                @Override
                public void run() {
                    for (int j = 0; j < Integer.MAX_VALUE; j++);
                    ProcStatUtil.ProcStat curr = null;
                    try {
                        curr = parseJiffiesInfoWithBufferForPathR2("/proc/" + Process.myPid() + "/stat");
                    } catch (ProcStatUtil.ParseException e) {
                        e.printStackTrace();
                    }
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
    public void testGetMyProcThreadStatOpt() throws ProcStatUtil.ParseException {
        String dirPath = "/proc/" + Process.myPid() + "/task";

        File[] tasksFile = new File(dirPath).listFiles();
        for (File item : tasksFile) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));

                ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBufferForPath(catPath, new byte[2 * 1024]);
                Assert.assertNotNull(stat.comm);
                Assert.assertNotNull(stat.stat);
                Assert.assertTrue("thread state is: " + stat.stat, Arrays.asList("R", "S").contains(stat.stat));
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
    public void testGetMyProcThreadStatOptR2() throws ProcStatUtil.ParseException {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));

                ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBufferForPathR2(catPath);
                Assert.assertNotNull(stat.comm);
                Assert.assertNotNull(stat.stat);
                Assert.assertTrue(Arrays.asList("R", "S").contains(stat.stat));
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
    public void testGetMyProcThreadStatAndCompare() throws ProcStatUtil.ParseException {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));
                ProcStatUtil.ProcStat statInfo1 = ProcStatUtil.parseWithSplits(cat);
                ProcStatUtil.ProcStat statInfo2 = parseJiffiesInfoWithBuffer(cat.getBytes());
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.stat, statInfo2.stat);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);

                cat = getProStatText(catPath);
                statInfo1 = ProcStatUtil.parseWithSplits(cat);
                statInfo2 = parseJiffiesInfoWithBuffer(cat.getBytes());
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.stat, statInfo2.stat);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);
            }
        }
    }

    @Test
    public void testReadBuffer() throws ProcStatUtil.ParseException {
        String sample = "10966 (terycanary.test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 33 3";
        Assert.assertEquals(72, sample.length());
        Assert.assertEquals(72, sample.getBytes().length);

        ProcStatUtil.ProcStat stat = parseJiffiesInfoWithBuffer(sample.getBytes());
        Assert.assertEquals("terycanary.test", stat.comm);
        Assert.assertEquals("S", stat.stat);
        Assert.assertEquals(22, stat.utime);
        Assert.assertEquals(2, stat.stime);
        Assert.assertEquals(33, stat.cutime);
        Assert.assertEquals(3, stat.cstime);
        long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertEquals(22 + 2 + 33 + 3, jiffies);

        sample = "10966 (terycanary test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 33 3";
        stat = parseJiffiesInfoWithBuffer(sample.getBytes());
        Assert.assertEquals("terycanary test", stat.comm);
        Assert.assertEquals("S", stat.stat);
        Assert.assertEquals(22, stat.utime);
        Assert.assertEquals(2, stat.stime);
        Assert.assertEquals(33, stat.cutime);
        Assert.assertEquals(3, stat.cstime);
        jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertEquals(22 + 2 + 33 + 3, jiffies);

        sample = "10966 (tery canary test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 33 3";
        stat = parseJiffiesInfoWithBuffer(sample.getBytes());
        Assert.assertEquals("tery canary test", stat.comm);
        Assert.assertEquals("S", stat.stat);
        Assert.assertEquals(22, stat.utime);
        Assert.assertEquals(2, stat.stime);
        Assert.assertEquals(33, stat.cutime);
        Assert.assertEquals(3, stat.cstime);
        jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertEquals(22 + 2 + 33 + 3, jiffies);
    }

    @Test
    public void testParseExceptionListener() throws IOException {
        String sample = "10966 (terycanary.test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 x 33 3";
        Assert.assertEquals(72, sample.length());
        Assert.assertEquals(72, sample.getBytes().length);
        try {
            ProcStatUtil.parseWithBuffer(sample.getBytes());
            Assert.fail("should fail");
        } catch (ProcStatUtil.ParseException e) {
            e.printStackTrace();
        }
        try {
            ProcStatUtil.parseWithSplits(sample);
            Assert.fail("should fail");
        } catch (ProcStatUtil.ParseException e) {
            e.printStackTrace();
        }

        final AtomicInteger inc = new AtomicInteger();
        ProcStatUtil.setParseErrorListener(new ProcStatUtil.OnParseError() {
            @Override
            public void onError(int mode, String input) {
                inc.incrementAndGet();
            }
        });
        File tempFile = File.createTempFile("temp_", "_test_parse_proc_stat_" + System.currentTimeMillis());
        IOUtil.writeText(sample, tempFile);
        ProcStatUtil.ProcStat parse = ProcStatUtil.parse(tempFile.getPath());
        Assert.assertEquals(2, inc.get());
        Assert.assertNull(parse);
    }

    @SuppressWarnings("ConstantConditions")
    @Test
    public void testGetMyProcThreadStatAndCompare2() throws ProcStatUtil.ParseException {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));
                ProcStatUtil.ProcStat statInfo1 = parseJiffiesInfoWithBuffer(cat.getBytes());
                ProcStatUtil.ProcStat statInfo2 = parseJiffiesInfoWithBuffer(Arrays.copyOfRange(cat.getBytes(), 0, 127));
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.stat, statInfo2.stat);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);
            }
        }
    }

    static ProcStatUtil.ProcStat parseJiffiesInfoWithSplitsForPath(String path) throws ProcStatUtil.ParseException {
        return ProcStatUtil.parseWithSplits(BatteryCanaryUtil.cat(path));
    }

    static ProcStatUtil.ProcStat parseJiffiesInfoWithBufferForPath(String path, byte[] buffer) throws ProcStatUtil.ParseException {
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

    static ProcStatUtil.ProcStat parseJiffiesInfoWithBufferForPathR2(String path) throws ProcStatUtil.ParseException {
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

    static ProcStatUtil.ProcStat parseJiffiesInfoWithBuffer(byte[] statBuffer) throws ProcStatUtil.ParseException {
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

    @Test
    public void testGetProcStatR3() throws ProcStatUtil.ParseException {
        ProcStatUtil.ProcStat procStat = ProcStatUtil.BetterProcStatParser.parse("/proc/" + Process.myPid() + "/stat");
        Assert.assertNotNull(procStat);
    }


    @RunWith(AndroidJUnit4.class)
    public static class Benchmark {
        static final String TAG = "Matrix.test.ProcessCpuTrackUtilsTest$Benchmark";

        Context mContext;

        @Before
        public void setUp() {
            mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
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
        public void testGetMyProcThreadStatBenchmark() throws ProcStatUtil.ParseException {
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
        public void testGetMyProcThreadStatWithBufferBenchmark() throws ProcStatUtil.ParseException {
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
            mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
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


    @RunWith(AndroidJUnit4.class)
    public static class IndividualCases {
        static final String TAG = "Matrix.test.ProcessCpuTrackUtilsTest$MultiProcess";

        Context mContext;

        @Before
        public void setUp() {
            mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        }

        @After
        public void shutDown() {
        }

        @Test
        public void testConcurrentParsing() throws IOException, ProcStatUtil.ParseException, InterruptedException {
            ProcStatUtil.setParseErrorListener(new ProcStatUtil.OnParseError() {
                @Override
                public void onError(final int mode, final String input) {
                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                        @Override
                        public void run() {
                            Assert.fail("mode = " + mode + ", error = " + input);
                        }
                    });
                }
            });

            List<Thread> threadList = new ArrayList<>();
            for (int i = 0; i < 1000; i++) {
                Thread thread = new Thread(new Runnable() {
                    @Override
                    public void run() {
                        ProcStatUtil.ProcStat computedStat = ProcStatUtil.of(Process.myTid());
                        Assert.assertNotNull(computedStat);
                    }
                });
                thread.start();
                threadList.add(thread);
            }

            for (Thread item : threadList) {
                item.join();
            }
        }

        @Test
        public void testParsingIndividualCases() throws IOException, ProcStatUtil.ParseException {
            List<String> cases = Arrays.asList(
                    "30109 ([GT]ColdPool#4) R 631 808 0 0 -1 4210752 2491 1245 0 0 48 15 0 1 30 10 179 0 23003491 44575170560 143434 18446744073709551615 419586322432 419586347040 548986306016 0 0 0 4608 4097 1073775868 0 0 0 -1 7 0 0 3 0 0 419586351104 419586352512 419837206528 548986310249 548986310326 548986310326 548986314718 0 0 0 8 0",
                    "30712 ([GT]HotPool#1) R 631 808 0 0 -1 4210752 1858 1245 0 0 27 9 0 1 16 -4 173 0 23004238 44357824512 122121 18446744073709551615 419586322432 419586347040 548986306016 0 0 0 4612 4097 1073775868 0 0 0 -1 2 0 0 1 0 0 419586351104 419586352512 419837206528 548986310249 548986310326 548986310326 548986314718 0 0 0 0 0 0",
                    "11522 (default_matrix_) R 1670 1670 0 0 -1 4210752 45502 581 1275 0 1214 1521 0 0 20 0 173 0 14969918 14697058304 67774 18446744073709551615 373188939776 373188964816 549282126752 0 0 0 4612 4097 1073775868 0 0 0 -1 6 0 0 0 0 0 373188968448 373188969896 373791416320 549282129081 549282129180 549282129180 549282131934 0",
                    "20764:11151 (default_matrix_) R 634 810 0 0 -1 4210752 363390 25980 11017 62 9807 10924 19 14 20 0 201 0 7580454 15621533696 88111 18446744073709551615 387170705408 387170730016 549146763456 0 0 0 4612 4097 1073775868 0 0 0 -1 4 0 0 141 0 0 387170734080 387170735488 388046045184 549146766953 549146767030 549146767030 549146771422 0",
                    "1542:20423 (default_matrix_) R 655 655 0 0 -1 4210752 32711 5909 4 4 676 856 3 7 20 0 199 0 17686272 15023083520 151543 18446744073709551615 399329419264 399329443936 548751576320 0 0 0 4612 4097 1073775868 0 0 0 -1 5 0 0 0 0 0 399329447936 399329449344 400316149760 548751580613 548751580712 548751580712 548751585246 0",
                    "6057:5761 (default_matrix_) R 619 764 0 0 -1 4210752 3114 9540 77 13 3786 2261 4 6 20 0 213 0 99587445 12487331840 57725 18446744073709551615 405469245440 405469263388 549287708384 0 0 0 4612 4097 1073775868 0 0 0 -1 6 0 0 4 0 0 405469375048 405469376512 405563015168 549287712662 549287712739 549287712739 549287714782 0",
                    "4712:24905 (default_matrix_) R 678 910 0 0 -1 4210752 136335 18833 44 26 2525 2167 7 13 20 0 201 0 890128192 16258711552 42328 18446744073709551615 396501762048 396501786656 549401098288 0 0 0 4612 4097 1073775868 0 0 0 -1 4 0 0 1 0 0 396501790720 396501792128 397494910976 549401099883 549401099960 549401099960 549401104350 0"
            );

            for (String cat : cases) {
                ProcStatUtil.ProcStat exceptedStat = ProcStatUtil.parseWithSplits(cat);
                Assert.assertNotNull(exceptedStat);
                File tempFile = File.createTempFile("temp_", "_test_parse_proc_stat_" + System.currentTimeMillis());
                IOUtil.writeText(cat, tempFile);
                ProcStatUtil.ProcStat computedStat = ProcStatUtil.parse(tempFile.getPath());
                Assert.assertNotNull(computedStat);

                Assert.assertEquals(exceptedStat.getJiffies(), computedStat.getJiffies());
                Assert.assertEquals(exceptedStat.comm, computedStat.comm);
            }
        }
    }
}
