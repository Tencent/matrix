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

package com.tencent.matrix.batterycanary;

import android.content.Context;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;

import com.tencent.matrix.batterycanary.utils.ProcStatUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.Stack;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;


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
    public void testReadPowerProfile() throws ClassNotFoundException, NoSuchMethodException, IllegalAccessException, InvocationTargetException, InstantiationException {
        Class<?> clazz = Class.forName("com.android.internal.os.PowerProfile");
        Constructor<?> constructor = clazz.getConstructor(Context.class);
        Object obj = constructor.newInstance(mContext);
        Assert.assertNotNull(obj);
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
        Assert.fail(sb.toString());
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
        Assert.fail(sb.toString());
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
