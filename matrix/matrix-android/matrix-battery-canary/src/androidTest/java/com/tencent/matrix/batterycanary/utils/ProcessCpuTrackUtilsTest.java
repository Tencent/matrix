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
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import android.text.TextUtils;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

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
                ProcStatInfo statInfo1 = parseJiffiesInfoWithSplits(cat);
                ProcStatInfo statInfo2 = parseJiffiesInfoWithBuffer(cat.getBytes());
                Assert.assertEquals(statInfo1.comm, statInfo2.comm);
                Assert.assertEquals(statInfo1.utime, statInfo2.utime);
                Assert.assertEquals(statInfo1.stime, statInfo2.stime);
                Assert.assertEquals(statInfo1.cutime, statInfo2.cutime);
                Assert.assertEquals(statInfo1.cstime, statInfo2.cstime);

                cat = getProStatText(catPath);
                statInfo1 = parseJiffiesInfoWithSplits(cat);
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

    public static class ProcStatInfo {
        @Nullable
        public String comm;
        public long utime = -1;
        public long stime = -1;
        public long cutime = -1;
        public long cstime = -1;
    }

    static ProcStatInfo parseJiffiesInfoWithSplitsForPath(String path) {
        return parseJiffiesInfoWithSplits(BatteryCanaryUtil.cat(path));
    }

    static ProcStatInfo parseJiffiesInfoWithSplits(String cat) {
        // cat = cat.substring(0, cat.length() / 2);
        ProcStatInfo stat = new ProcStatInfo();
        if (!TextUtils.isEmpty(cat)) {
            String[] splits = cat.split(" ");
            String name = splits[1];
            if (name.startsWith("(")) name = name.substring(1);
            if (name.endsWith(")")) name = name.substring(0, name.length() - 1);
            stat.comm = name;
            stat.utime = MatrixUtil.parseLong(splits[13], 0);
            stat.stime = MatrixUtil.parseLong(splits[14], 0);
            stat.cutime = MatrixUtil.parseLong(splits[15], 0);
            stat.cstime = MatrixUtil.parseLong(splits[16], 0);
        }
        return stat;
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
                    if (spaceCount > 16) {
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
        /*
         * 样本:
         * 10966 (terycanary.test) S 699 699 0 0 -1 1077952832 6187 0 0 0 22 2 0 0 20 0 17 0 9087400 5414273024
         *  24109 18446744073709551615 421814448128 421814472944 549131058960 0 0 0 4612 1 1073775864
         *  1 0 0 17 7 0 0 0 0 0 421814476800 421814478232 422247952384 549131060923 549131061022 549131061022
         *  549131063262 0
         *
         * 字段:
         * - pid:  进程ID.
         * - comm: task_struct结构体的进程名
         * - state: 进程状态, 此处为S
         * - ppid: 父进程ID （父进程是指通过fork方式, 通过clone并非父进程）
         * - pgrp: 进程组ID
         * - session: 进程会话组ID
         * - tty_nr: 当前进程的tty终点设备号
         * - tpgid: 控制进程终端的前台进程号
         * - flags: 进程标识位, 定义在include/linux/sched.h中的PF_*, 此处等于1077952832
         * - minflt:  次要缺页中断的次数, 即无需从磁盘加载内存页. 比如COW和匿名页
         * - cminflt: 当前进程等待子进程的minflt
         * - majflt: 主要缺页中断的次数, 需要从磁盘加载内存页. 比如map文件
         * - majflt: 当前进程等待子进程的majflt
         * - utime: 该进程处于用户态的时间, 单位jiffies, 此处等于166114
         * - stime: 该进程处于内核态的时间, 单位jiffies, 此处等于129684
         * - cutime: 当前进程等待子进程的utime
         * - cstime: 当前进程等待子进程的utime
         * - priority: 进程优先级, 此次等于10.
         * - nice: nice值, 取值范围[19, -20], 此处等于-10
         * - num_threads: 线程个数, 此处等于221
         * - itrealvalue: 该字段已废弃, 恒等于0
         * - starttime: 自系统启动后的进程创建时间, 单位jiffies, 此处等于2284
         * - vsize: 进程的虚拟内存大小, 单位为bytes
         * - rss: 进程独占内存+共享库, 单位pages, 此处等于93087
         * - rsslim: rss大小上限
         *
         * 说明:
         * 第10~17行主要是随着时间而改变的量；
         * 内核时间单位, sysconf(_SC_CLK_TCK)一般地定义为jiffies(一般地等于10ms)
         * starttime: 此值单位为jiffies, 结合/proc/stat的btime, 可知道每一个线程启动的时间点
         * 1500827856 + 2284/100 = 1500827856, 转换成北京时间为2017/7/24 0:37:58
         * 第四行数据很少使用,只说一下该行第7至9个数的含义:
         * signal: 即将要处理的信号, 十进制, 此处等于6660
         * blocked: 阻塞的信号, 十进制
         * sigignore: 被忽略的信号, 十进制, 此处等于36088
         */

        ProcStatInfo stat = new ProcStatInfo();
        int statBytes = statBuffer.length;
        for (int i = 0, spaceIdx = 0; i < statBytes; ) {
            if (Character.isSpaceChar(statBuffer[i])) {
                spaceIdx++;
                i++;
                continue;
            }

            switch (spaceIdx) {
                case 1: { // read comm (thread name)
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    for (; i < statBytes && !Character.isSpaceChar(statBuffer[i]); i++, window++) ;
                    if ('(' == statBuffer[readIdx]) {
                        readIdx++;
                        window--;
                    }
                    if (')' == statBuffer[readIdx + window - 1]) {
                        window--;
                    }
                    if (window > 0) {
                        stat.comm = new String(statBuffer, readIdx, window);
                    }
                    break;
                }

                case 13: { // utime
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    for (; i < statBytes && !Character.isSpaceChar(statBuffer[i]); i++, window++) ;
                    String num = new String(statBuffer, readIdx, window);
                    stat.utime = MatrixUtil.parseLong(num, 0);
                    break;
                }
                case 14: { // stime
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    for (; i < statBytes && !Character.isSpaceChar(statBuffer[i]); i++, window++) ;
                    String num = new String(statBuffer, readIdx, window);
                    stat.stime = MatrixUtil.parseLong(num, 0);
                    break;
                }
                case 15: { // cutime
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    for (; i < statBytes && !Character.isSpaceChar(statBuffer[i]); i++, window++) ;
                    String num = new String(statBuffer, readIdx, window);
                    stat.cutime = MatrixUtil.parseLong(num, 0);
                    break;
                }
                case 16: { // cstime
                    int readIdx = i, window = 0;
                    // seek next space
                    // noinspection StatementWithEmptyBody
                    for (; i < statBytes && !Character.isSpaceChar(statBuffer[i]); i++, window++) ;
                    String num = new String(statBuffer, readIdx, window);
                    stat.cstime = MatrixUtil.parseLong(num, 0);
                    break;
                }

                default:
                    i++;
            }
        }

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
