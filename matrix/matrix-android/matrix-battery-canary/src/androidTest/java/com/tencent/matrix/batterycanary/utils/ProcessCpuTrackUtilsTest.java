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

import android.content.Context;
import android.os.Process;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.TestUtils;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ProcStatInfo;
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

/**
 * Instrumented test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@SuppressWarnings("SpellCheckingInspection")
@RunWith(AndroidJUnit4.class)
public class ProcessCpuTrackUtilsTest {
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

    /**
     * cat: /proc/loadavg
     */
    @Test
    public void testGetCpuLoad() {
        String cat = BatteryCanaryUtil.cat("/proc/loadavg");
        Assert.assertTrue(TextUtils.isEmpty(cat));
    }

    @Test
    public void testGetCpuLoad2() {
        ProcessCpuTrackUtils.CpuLoad cpuLoad = ProcessCpuTrackUtils.getCpuLoad();
        Assert.assertNotNull(cpuLoad);
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
        String cat = BatteryCanaryUtil.cat("/proc/" + Process.myPid() + "/stat");
        Assert.assertFalse(TextUtils.isEmpty(cat));
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

                ProcStatInfo stat = parseJiffiesInfoWithSplitsForPath(catPath);
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
    public void testGetMyProcThreadStatOpt() {
        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                String catPath = new File(item, "stat").getAbsolutePath();
                String cat = BatteryCanaryUtil.cat(catPath);
                Assert.assertFalse(TextUtils.isEmpty(cat));

                ProcStatInfo stat = parseJiffiesInfoWithBufferForPath(catPath, new byte[2 * 1024]);
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

                ProcStatInfo stat = parseJiffiesInfoWithBufferForPathR2(catPath);
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
                ProcStatInfo statInfo1 = ProcStatInfo.parseJiffiesInfoWithSplits(cat);
                ProcStatInfo statInfo2 = parseJiffiesInfoWithBuffer(cat.getBytes());
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);

                cat = getProStatText(catPath);
                statInfo1 = ProcStatInfo.parseJiffiesInfoWithSplits(cat);
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
    public void testReadBuffer() {
        String sample = "10966 (terycanary.test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 33 3";
        Assert.assertEquals(72, sample.length());
        Assert.assertEquals(72, sample.getBytes().length);

        ProcStatInfo stat = parseJiffiesInfoWithBuffer(sample.getBytes());
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
                ProcStatInfo statInfo1 = parseJiffiesInfoWithBuffer(cat.getBytes());
                ProcStatInfo statInfo2 = parseJiffiesInfoWithBuffer(Arrays.copyOfRange(cat.getBytes(), 0, 127));
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);
            }
        }
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
                    ProcStatInfo stat = parseJiffiesInfoWithBufferForPath(catPath, new byte[2 * 1024]);
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
                    ProcStatInfo stat = parseJiffiesInfoWithBufferForPath(catPath, new byte[128]);
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


    static ProcStatInfo parseJiffiesInfoWithSplitsForPath(String path) {
        return ProcStatInfo.parseJiffiesInfoWithSplits(BatteryCanaryUtil.cat(path));
    }

    static ProcStatInfo parseJiffiesInfoWithBufferForPath(String path, byte[] buffer) {
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

    static ProcStatInfo parseJiffiesInfoWithBufferForPathR2(String path) {
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

    static ProcStatInfo parseJiffiesInfoWithBuffer(byte[] statBuffer) {
        ProcStatInfo stat = ProcStatInfo.parseJiffiesInfoWithBuffer(statBuffer);

        Assert.assertNotNull(stat.comm);
        Assert.assertTrue(stat.utime >= 0);
        Assert.assertTrue(stat.stime >= 0);
        Assert.assertTrue(stat.cutime >= 0);
        Assert.assertTrue(stat.cstime >= 0);

        long jiffies = stat.utime + stat.stime + stat.cutime + stat.cstime;
        Assert.assertTrue(jiffies >= 0);
        return stat;
    }
}
