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
import android.content.res.XmlResourceParser;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;

import com.tencent.matrix.batterycanary.TestUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.xmlpull.v1.XmlPullParser;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Stack;
import java.util.concurrent.atomic.AtomicInteger;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.JIFFY_MILLIS;
import static com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil.ONE_HOR;


@RunWith(AndroidJUnit4.class)
public class BatteryMetricsTest {
    static final String TAG = "Matrix.test.metrics";

    Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @After
    public void shutDown() {
    }

    @Test
    public void testReadPowerProfile() throws Exception {
        Class<?> clazz = Class.forName("com.android.internal.os.PowerProfile");
        Constructor<?> constructor = clazz.getConstructor(Context.class);
        Object obj = constructor.newInstance(mContext);
        Assert.assertNotNull(obj);

        int id = mContext.getResources().getIdentifier("power_profile", "xml", "android");
        Assert.assertTrue(id > 0);

        try (XmlResourceParser parser = mContext.getResources().getXml(id)) {
            while (parser.getEventType() != XmlPullParser.END_DOCUMENT) {
                if (parser.getEventType() == XmlPullParser.START_TAG) {
                    String tagName = parser.getName();
                    String tagText = parser.getText();
                    for (int i = 0; i < parser.getAttributeCount(); i++) {
                        String attrName = parser.getAttributeName(i);
                        String attrValue = parser.getAttributeValue(i);
                        if (attrValue.startsWith("cpu.core_speeds.cluster")) {
                        }
                    }
                }
                parser.nextToken();
            }
        }

        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertNotNull(PowerProfile.getInstance());
        Assert.assertTrue(PowerProfile.getInstance().isSupported());
    }

    @Test
    public void testGetAppCpuCoreNumByTid() {
        StringBuilder tips = new StringBuilder("\n");
        int num = configureCpuCoreNum(Process.myTid());
        tips.append(Thread.currentThread().getName()).append("(").append(Process.myTid()).append(") is running on cpu" + num).append("\n");

        String dirPath = "/proc/" + Process.myPid() + "/task";
        for (File item : new File(dirPath).listFiles()) {
            if (item.isDirectory()) {
                int tid = Integer.parseInt(item.getName());
                ProcStatUtil.ProcStat stat = ProcStatUtil.of(Process.myPid(), tid);
                num = configureCpuCoreNum(tid);
                tips.append(stat.comm).append("(").append(tid).append(") is running on cpu" + num).append("\n");
            }
        }

        if (!TestUtils.isAssembleTest()) {
            Assert.fail(tips.toString());
        }
    }

    @Test
    public void testCpuCoreChangingWithRunningThread() throws InterruptedException {
        StringBuilder tips = new StringBuilder("\n");
        final AtomicInteger tid = new AtomicInteger(-1);
        new Thread(new Runnable() {
            @Override
            public void run() {
                tid.set(Process.myTid());
                while (true) {}
            }
        }, "TEST_THREAD").start();

        while (tid.get() < 0) {}
        for (int i = 0; i < 100; i++) {
            int num = configureCpuCoreNum(tid.get());
            tips.append(num).append("\n");
            Thread.sleep(100L);
        }

        if (!TestUtils.isAssembleTest()) {
            Assert.fail(tips.toString());
        }
    }

    @Test
    public void testCpuCoreChangingWithBlockThread() throws InterruptedException {
        StringBuilder tips = new StringBuilder("\n");
        final AtomicInteger tid = new AtomicInteger(-1);
        new Thread(new Runnable() {
            @Override
            public void run() {
                tid.set(Process.myTid());
                synchronized (tid) {
                    try {
                        tid.wait();
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                }
            }
        }, "TEST_THREAD").start();

        while (tid.get() < 0) {}
        for (int i = 0; i < 100; i++) {
            int num = configureCpuCoreNum(tid.get());
            tips.append(num).append("\n");
            Thread.sleep(100L);
        }

        if (!TestUtils.isAssembleTest()) {
            Assert.fail(tips.toString());
        }
    }

    @Test
    public void testCpuCoreChangingWithUIThread() throws InterruptedException {
        StringBuilder tips = new StringBuilder("\n");
        final AtomicInteger tid = new AtomicInteger(-1);
        final Handler handler = new Handler(Looper.getMainLooper());
        handler.post(new Runnable() {
            @Override
            public void run() {
                tid.set(Process.myTid());
            }
        });

        handler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    Thread.sleep(50L);
                } catch (InterruptedException ignored) {
                }
                handler.post(this);
            }
        });

        while (tid.get() < 0) {}
        for (int i = 0; i < 100; i++) {
            int num = configureCpuCoreNum(tid.get());
            tips.append(num).append("\n");
            Thread.sleep(100L);
        }

        if (!TestUtils.isAssembleTest()) {
            Assert.fail(tips.toString());
        }
    }

    private static int configureCpuCoreNum(int tid) {
        String cat = BatteryCanaryUtil.cat("proc/" + Process.myPid() + "/task/" + tid + "/stat");
        Assert.assertFalse(TextUtils.isEmpty(cat));
        int index = cat.indexOf(")");
        if (index <= 0) throw new IllegalStateException(cat + " has not ')'");
        String suffix = cat.substring(index + ")".length());
        String[] splits = suffix.split(" ");
        int columnIndex = 37;
        Assert.assertTrue(splits.length >= columnIndex + 1);
        Assert.assertTrue(ProcStatUtil.isNumeric(splits[columnIndex]));
        Assert.assertTrue(splits[columnIndex] + ": tid = " + tid, TextUtils.isDigitsOnly(splits[columnIndex]));
        return Integer.parseInt(splits[columnIndex]);
    }

    @Test
    public void testReadKernelCpuSpeedState() throws IOException {
        for (int i = 0; i < BatteryCanaryUtil.getCpuCoreNum(); i++) {
            String path = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/stats/time_in_state";
            try (BufferedReader reader = new BufferedReader(new FileReader(new File(path)))) {
                TextUtils.SimpleStringSplitter splitter = new TextUtils.SimpleStringSplitter(' ');
                String line;
                while ((line = reader.readLine()) != null) {
                    splitter.setString(line);
                    String speed = splitter.next();
                    String time = splitter.next();
                    Assert.assertTrue(TextUtils.isDigitsOnly(speed));
                    Assert.assertTrue(TextUtils.isDigitsOnly(time));
                }
            }
        }

        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        KernelCpuSpeedReader[] readers = new KernelCpuSpeedReader[powerProfile.getNumCpuClusters()];
        int firstCpuOfCluster = 0;
        for (int i = 0; i < powerProfile.getNumCpuClusters(); i++) {
            final int numSpeedSteps = powerProfile.getNumSpeedStepsInCpuCluster(i);
            readers[i] = new KernelCpuSpeedReader(firstCpuOfCluster, numSpeedSteps);
            firstCpuOfCluster += powerProfile.getNumCoresInCpuCluster(i);
        }

        for (int i = 0; i < readers.length; i++) {
            KernelCpuSpeedReader kernelCpuSpeedReader = readers[i];
            Assert.assertNotNull(kernelCpuSpeedReader);
            Assert.assertTrue(kernelCpuSpeedReader.readTotoal() > 0);
            long[] cpuCoreJiffies = kernelCpuSpeedReader.readAbsolute();
            Assert.assertEquals(powerProfile.getNumSpeedStepsInCpuCluster(i), cpuCoreJiffies.length);
            for (int j = 0; j < cpuCoreJiffies.length; j++) {
                Assert.assertTrue(cpuCoreJiffies[j] >= 0);
            }
        }
    }

    @Test
    public void testConfigureCpuLoad() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        int cpuCoreNum = BatteryCanaryUtil.getCpuCoreNum();
        int cpuCoreNum2 = 0;
        for (int i = 0; i < powerProfile.getNumCpuClusters(); i++) {
            cpuCoreNum2 += powerProfile.getNumCoresInCpuCluster(i);
        }
        Assert.assertEquals(cpuCoreNum2, cpuCoreNum);

        KernelCpuSpeedReader[] readers = new KernelCpuSpeedReader[cpuCoreNum];
        for (int i = 0; i < cpuCoreNum; i++) {
            final int numSpeedSteps = powerProfile.getNumSpeedStepsInCpuCluster(powerProfile.getClusterByCpuNum(i));
            readers[i] = new KernelCpuSpeedReader(i, numSpeedSteps);
        }

        long cpuJiffiesBgn = 0;
        for (int i = 0; i < readers.length; i++) {
            KernelCpuSpeedReader kernelCpuSpeedReader = readers[i];
            Assert.assertNotNull(kernelCpuSpeedReader);
            long cpuCoreJiffies = kernelCpuSpeedReader.readTotoal();
            Assert.assertTrue("idx = " + i + ",  read = " + Arrays.toString(kernelCpuSpeedReader.readAbsolute()), cpuCoreJiffies > 0);
            cpuJiffiesBgn += cpuCoreJiffies;
        }
        long appJiffiesBgn = ProcStatUtil.of(Process.myPid()).getJiffies();

        CpuConsumption.hanoi(20);

        long cpuJiffiesEnd = 0;
        for (int i = 0; i < readers.length; i++) {
            KernelCpuSpeedReader kernelCpuSpeedReader = readers[i];
            Assert.assertNotNull(kernelCpuSpeedReader);
            long cpuCoreJiffies = kernelCpuSpeedReader.readTotoal();
            Assert.assertTrue(cpuCoreJiffies > 0);
            cpuJiffiesEnd += cpuCoreJiffies;
        }
        long appJiffiesEnd = ProcStatUtil.of(Process.myPid()).getJiffies();

        long cpuJiffiesDelta = cpuJiffiesEnd - cpuJiffiesBgn;
        long appJiffiesDelta = appJiffiesEnd - appJiffiesBgn;
        Assert.assertTrue(cpuJiffiesDelta > 0);
        Assert.assertTrue(appJiffiesDelta > 0);
        Assert.assertTrue(appJiffiesDelta < cpuJiffiesDelta);

        float cpuLoad = (float) appJiffiesDelta / cpuJiffiesDelta;
        float cpuLoadAvg = cpuLoad * cpuCoreNum;

        if (!TestUtils.isAssembleTest()) {
            Assert.fail("CPU Load: " + (int) (cpuLoadAvg * 100) + "%");
        }
    }

    @Test
    public void testConfigureCpuLoadWithMultiThread() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        int cpuCoreNum = BatteryCanaryUtil.getCpuCoreNum();
        int cpuCoreNum2 = 0;
        for (int i = 0; i < powerProfile.getNumCpuClusters(); i++) {
            cpuCoreNum2 += powerProfile.getNumCoresInCpuCluster(i);
        }
        Assert.assertEquals(cpuCoreNum2, cpuCoreNum);

        KernelCpuSpeedReader[] readers = new KernelCpuSpeedReader[cpuCoreNum];
        for (int i = 0; i < cpuCoreNum; i++) {
            final int numSpeedSteps = powerProfile.getNumSpeedStepsInCpuCluster(powerProfile.getClusterByCpuNum(i));
            readers[i] = new KernelCpuSpeedReader(i, numSpeedSteps);
        }

        long cpuJiffiesBgn = 0;
        for (int i = 0; i < readers.length; i++) {
            KernelCpuSpeedReader kernelCpuSpeedReader = readers[i];
            Assert.assertNotNull(kernelCpuSpeedReader);
            long cpuCoreJiffies = kernelCpuSpeedReader.readTotoal();
            Assert.assertTrue("idx = " + i + ",  read = " + Arrays.toString(kernelCpuSpeedReader.readAbsolute()), cpuCoreJiffies > 0);
            cpuJiffiesBgn += cpuCoreJiffies;
        }
        long appJiffiesBgn = ProcStatUtil.of(Process.myPid()).getJiffies();

        for (int i = 0; i < cpuCoreNum - 1; i++) {
            new Thread(new Runnable() {
                @Override
                public void run() {
                    while (true) {}
                }
            }).start();
        }
        CpuConsumption.hanoi(20);

        long cpuJiffiesEnd = 0;
        for (int i = 0; i < readers.length; i++) {
            KernelCpuSpeedReader kernelCpuSpeedReader = readers[i];
            Assert.assertNotNull(kernelCpuSpeedReader);
            long cpuCoreJiffies = kernelCpuSpeedReader.readTotoal();
            Assert.assertTrue(cpuCoreJiffies > 0);
            cpuJiffiesEnd += cpuCoreJiffies;
        }
        long appJiffiesEnd = ProcStatUtil.of(Process.myPid()).getJiffies();

        long cpuJiffiesDelta = cpuJiffiesEnd - cpuJiffiesBgn;
        long appJiffiesDelta = appJiffiesEnd - appJiffiesBgn;
        Assert.assertTrue(cpuJiffiesDelta > 0);
        Assert.assertTrue(appJiffiesDelta > 0);
        Assert.assertTrue(appJiffiesDelta < cpuJiffiesDelta);

        float cpuLoad = (float) appJiffiesDelta / cpuJiffiesDelta;
        float cpuLoadAvg = cpuLoad * cpuCoreNum;

        if (!TestUtils.isAssembleTest()) {
            Assert.fail("CPU Load: " + (int) (cpuLoadAvg * 100) + "%");
        }
    }

    @Test
    public void testGetCpuPowers() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        double[] clusterPowers = new double[powerProfile.getNumCpuClusters()];
        for (int i = 0; i < powerProfile.getNumCpuClusters(); i++) {
            double clusterPower = powerProfile.getAveragePowerForCpuCluster(i);
            clusterPowers[i] = clusterPower;
        }

        List<double[]> cpuCoreStepPowers = new ArrayList<>();
        for (int i = 0; i < powerProfile.getCpuCoreNum(); i++) {
            int cluster = powerProfile.getClusterByCpuNum(i);
            int steps = powerProfile.getNumSpeedStepsInCpuCluster(cluster);
            double[] stepPowers = new double[steps];
            for (int step = 0; step < steps; step++) {
                stepPowers[step] = powerProfile.getAveragePowerForCpuCore(cluster, step);
            }
            cpuCoreStepPowers.add(stepPowers);
        }

        for (double[] stepPowers : cpuCoreStepPowers) {
            for (double item : stepPowers) {
                Assert.assertTrue(item > 0);
            }
        }
    }

    @Test
    public void testConfigureCpuBatterySipping() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        int cpuCoreNum = powerProfile.getCpuCoreNum();
        List<double[]> cpuCoreStepPowers = new ArrayList<>();
        for (int i = 0; i < cpuCoreNum; i++) {
            int cluster = powerProfile.getClusterByCpuNum(i);
            int steps = powerProfile.getNumSpeedStepsInCpuCluster(cluster);
            double[] stepPowers = new double[steps];
            for (int step = 0; step < steps; step++) {
                stepPowers[step] = powerProfile.getAveragePowerForCpuCore(cluster, step);
            }
            cpuCoreStepPowers.add(stepPowers);
        }

        for (double[] stepPowers : cpuCoreStepPowers) {
            for (double item : stepPowers) {
                Assert.assertTrue(item > 0);
            }
        }

        KernelCpuSpeedReader[] readers = new KernelCpuSpeedReader[cpuCoreNum];
        for (int i = 0; i < cpuCoreNum; i++) {
            final int numSpeedSteps = powerProfile.getNumSpeedStepsInCpuCluster(powerProfile.getClusterByCpuNum(i));
            readers[i] = new KernelCpuSpeedReader(i, numSpeedSteps);
        }

        long jiffiesSum = 0;
        List<double[]> cpuCoreStepSips = new ArrayList<>();
        Assert.assertEquals(cpuCoreStepPowers.size(), readers.length);
        for (int i = 0; i < readers.length; i++) {
            double[] stepPowers = cpuCoreStepPowers.get(i);
            KernelCpuSpeedReader reader = readers[i];
            long[] stepJiffies = reader.readAbsolute();
            Assert.assertEquals(stepPowers.length, stepJiffies.length);
            double[] stepSips = new double[stepJiffies.length];
            for (int j = 0; j < stepJiffies.length; j++) {
                long jiffies = stepJiffies[j];
                jiffiesSum += jiffies;
                double power = stepPowers[j];
                double sip = power * ((double) jiffies * JIFFY_MILLIS / ONE_HOR);
                stepSips[j] = sip;
            }
            cpuCoreStepSips.add(stepSips);
        }

        double sipSum = 0;
        for (double[] stepSips : cpuCoreStepSips) {
            for (double item : stepSips) {
                Assert.assertTrue(item >= 0);
                sipSum += item;
            }
        }
        Assert.assertTrue(sipSum >= 0);
        if (!TestUtils.isAssembleTest()) {
            Assert.fail("CPU JiffyHour: " + ((float) (jiffiesSum) * JIFFY_MILLIS) / ONE_HOR
                    + "\nCPU Sipping: " + sipSum + " mAh");
        }
    }

    @Test
    public void testConfigureCpuBatterySippingDelta() throws IOException {
        PowerProfile powerProfile = PowerProfile.init(mContext);
        Assert.assertNotNull(powerProfile);
        Assert.assertTrue(powerProfile.isSupported());

        int cpuCoreNum = powerProfile.getCpuCoreNum();
        List<double[]> cpuCoreStepPowers = new ArrayList<>();
        for (int i = 0; i < cpuCoreNum; i++) {
            int cluster = powerProfile.getClusterByCpuNum(i);
            int steps = powerProfile.getNumSpeedStepsInCpuCluster(cluster);
            double[] stepPowers = new double[steps];
            for (int step = 0; step < steps; step++) {
                stepPowers[step] = powerProfile.getAveragePowerForCpuCore(cluster, step);
            }
            cpuCoreStepPowers.add(stepPowers);
        }

        for (double[] stepPowers : cpuCoreStepPowers) {
            for (double item : stepPowers) {
                Assert.assertTrue(item > 0);
            }
        }

        KernelCpuSpeedReader[] readers = new KernelCpuSpeedReader[cpuCoreNum];
        for (int i = 0; i < cpuCoreNum; i++) {
            final int numSpeedSteps = powerProfile.getNumSpeedStepsInCpuCluster(powerProfile.getClusterByCpuNum(i));
            readers[i] = new KernelCpuSpeedReader(i, numSpeedSteps);
        }

        long cpuJiffiesBgn = 0;
        for (int i = 0; i < readers.length; i++) {
            KernelCpuSpeedReader kernelCpuSpeedReader = readers[i];
            Assert.assertNotNull(kernelCpuSpeedReader);
            long cpuCoreJiffies = kernelCpuSpeedReader.readTotoal();
            Assert.assertTrue("idx = " + i + ",  read = " + Arrays.toString(kernelCpuSpeedReader.readAbsolute()), cpuCoreJiffies > 0);
            cpuJiffiesBgn += cpuCoreJiffies;
        }
        long appJiffiesBgn = ProcStatUtil.of(Process.myPid()).getJiffies();

        List<long[]> cpuCoreStepJiffies = new ArrayList<>();
        for (int i = 0; i < readers.length; i++) {
            KernelCpuSpeedReader reader = readers[i];
            long[] stepJiffies = reader.readAbsolute();
            cpuCoreStepJiffies.add(stepJiffies);
        }

        CpuConsumption.hanoi(20);

        long cpuJiffiesEnd = 0;
        for (int i = 0; i < readers.length; i++) {
            KernelCpuSpeedReader kernelCpuSpeedReader = readers[i];
            Assert.assertNotNull(kernelCpuSpeedReader);
            long cpuCoreJiffies = kernelCpuSpeedReader.readTotoal();
            Assert.assertTrue(cpuCoreJiffies > 0);
            cpuJiffiesEnd += cpuCoreJiffies;
        }
        Assert.assertTrue(cpuJiffiesEnd > cpuJiffiesBgn);

        List<long[]> cpuCoreStepJiffiesDeltas = new ArrayList<>();
        for (int i = 0; i < readers.length; i++) {
            long[] stepJiffiesBgns = cpuCoreStepJiffies.get(i);
            KernelCpuSpeedReader reader = readers[i];
            long[] stepJiffies = reader.readAbsolute();
            Assert.assertEquals(stepJiffiesBgns.length, stepJiffies.length);
            long[] stepJiffiesDeltas = new long[stepJiffies.length];
            for (int step = 0; step < stepJiffies.length; step++) {
                long jiffiesBgn = stepJiffiesBgns[step];
                long jiffiesEnd = stepJiffies[step];
                stepJiffiesDeltas[step] = jiffiesEnd - jiffiesBgn;
            }
            cpuCoreStepJiffiesDeltas.add(stepJiffiesDeltas);
        }

        Assert.assertEquals(cpuCoreStepJiffiesDeltas.size(), readers.length);

        List<double[]> cpuCoreStepSips = new ArrayList<>();
        for (int i = 0; i < readers.length; i++) {
            double[] stepPowers = cpuCoreStepPowers.get(i);
            long[] stepJiffies = cpuCoreStepJiffiesDeltas.get(i);
            Assert.assertEquals(stepPowers.length, stepJiffies.length);
            double[] stepSips = new double[stepJiffies.length];
            for (int j = 0; j < stepJiffies.length; j++) {
                long jiffies = stepJiffies[j];
                double power = stepPowers[j];
                double sip = power * ((double) jiffies * JIFFY_MILLIS / ONE_HOR);
                stepSips[j] = sip;
            }
            cpuCoreStepSips.add(stepSips);
        }

        double sipSum = 0;
        for (double[] stepSips : cpuCoreStepSips) {
            for (double item : stepSips) {
                Assert.assertTrue(item >= 0);
                sipSum += item;
            }
        }
        Assert.assertTrue(sipSum >= 0);
        if (!TestUtils.isAssembleTest()) {
            Assert.fail("CPU JiffyHour: " + ((float) (cpuJiffiesEnd - cpuJiffiesBgn) * JIFFY_MILLIS) / ONE_HOR
                    + "\nCPU Sipping: " + sipSum + " mAh");
        }
    }

    @SuppressWarnings("SpellCheckingInspection")
    @Test
    public void testJiffiesConsumption() {
        long wallTimeBgn = System.currentTimeMillis();
        long upTimeBgn = SystemClock.uptimeMillis();
        long threadTimeBgn = SystemClock.currentThreadTimeMillis();

        ProcStatUtil.ProcStat procStatBgn = ProcStatUtil.of(Process.myPid(), Process.myTid());
        long threadJiffiesBgn = procStatBgn.getJiffies();
        long threadUtimeBgn = procStatBgn.utime;
        long threadStimeBgn = procStatBgn.stime;
        long threadCutimeBgn = procStatBgn.cutime;
        long threadCstimeBgn = procStatBgn.cstime;

        CpuConsumption.inc(Integer.MAX_VALUE);

        ProcStatUtil.ProcStat procStatEnd = ProcStatUtil.of(Process.myPid(), Process.myTid());
        StringBuilder sb = new StringBuilder().append("wallTimeDiff = ").append(System.currentTimeMillis() - wallTimeBgn)
                .append("\nupTimeDiff = ").append(SystemClock.uptimeMillis() - upTimeBgn)
                .append("\nthreadTimeDiff = ").append(SystemClock.currentThreadTimeMillis() - threadTimeBgn)
                .append("\nthreadJiffiesDiff = ").append(procStatEnd.getJiffies() - threadJiffiesBgn)
                .append("\nthreadUtimeDiff = ").append(procStatEnd.utime - threadUtimeBgn)
                .append("\nthreadStimeDiff = ").append(procStatEnd.stime - threadStimeBgn)
                .append("\nthreadCutimeDiff = ").append(procStatEnd.cutime - threadCutimeBgn)
                .append("\nthreadCstimeDiff = ").append(procStatEnd.cstime - threadCstimeBgn)
                .append("\n");
        if (!TestUtils.isAssembleTest()) {
            Assert.fail(sb.toString());
        }
    }

    @SuppressWarnings("SpellCheckingInspection")
    @Test
    public void testJiffiesConsumption2() {
        long wallTimeBgn = System.currentTimeMillis();
        long upTimeBgn = SystemClock.uptimeMillis();
        long threadTimeBgn = SystemClock.currentThreadTimeMillis();

        ProcStatUtil.ProcStat procStatBgn = ProcStatUtil.of(Process.myPid(), Process.myTid());
        long threadJiffiesBgn = procStatBgn.getJiffies();
        long threadUtimeBgn = procStatBgn.utime;
        long threadStimeBgn = procStatBgn.stime;
        long threadCutimeBgn = procStatBgn.cutime;
        long threadCstimeBgn = procStatBgn.cstime;

        CpuConsumption.hanoi(20);

        ProcStatUtil.ProcStat procStatEnd = ProcStatUtil.of(Process.myPid(), Process.myTid());
        StringBuilder sb = new StringBuilder().append("wallTimeDiff = ").append(System.currentTimeMillis() - wallTimeBgn)
                .append("\nupTimeDiff = ").append(SystemClock.uptimeMillis() - upTimeBgn)
                .append("\nthreadTimeDiff = ").append(SystemClock.currentThreadTimeMillis() - threadTimeBgn)
                .append("\nthreadJiffiesDiff = ").append(procStatEnd.getJiffies() - threadJiffiesBgn)
                .append("\nthreadUtimeDiff = ").append(procStatEnd.utime - threadUtimeBgn)
                .append("\nthreadStimeDiff = ").append(procStatEnd.stime - threadStimeBgn)
                .append("\nthreadCutimeDiff = ").append(procStatEnd.cutime - threadCutimeBgn)
                .append("\nthreadCstimeDiff = ").append(procStatEnd.cstime - threadCstimeBgn)
                .append("\n");
        if (!TestUtils.isAssembleTest()) {
            Assert.fail(sb.toString());
        }
    }

    public static class CpuConsumption {
        public static void inc(int num) {
            for (int i = 0; i < num; i++) {
            }
        }

        public static void hanoi(int num) {
            Stack<Integer> torre1 = new Stack<>();
            Stack<Integer> torre2 = new Stack<>();
            Stack<Integer> torre3 = new Stack<>();

            for (int i = num; i >= 1; i--) {
                torre1.push(i);
            }
            ordena(torre1.size(), torre1, torre3, torre2);
        }

        private static void ordena(int n, Stack<Integer> torre1, Stack<Integer> torre3, Stack<Integer> torre2) {
            if (n > 0) {
                ordena(n - 1, torre1, torre2, torre3);
                torre3.push(torre1.pop());
                Log.i(TAG, "Torre1: " + torre1 + " Torre 2: " + torre2 + " Torre3: " + torre3);
                ordena(n - 1, torre2, torre3, torre1);
            }
        }
    }
}
