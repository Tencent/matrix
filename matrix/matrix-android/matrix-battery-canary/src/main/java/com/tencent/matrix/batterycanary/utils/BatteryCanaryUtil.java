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
import android.app.AlarmManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.display.DisplayManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.view.Display;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.ListIterator;
import java.util.regex.Pattern;

import static android.content.Context.ACTIVITY_SERVICE;

/**
 * @author liyongjie
 *         Created by liyongjie on 2017/8/14.
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class BatteryCanaryUtil {
    private static final String TAG = "Matrix.battery.Utils";
    private static final int DEFAULT_MAX_STACK_LAYER = 10;

    public static String getProcessName() {
        BatteryMonitorPlugin plugin = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
        if (plugin == null) {
            throw new IllegalStateException("BatteryMonitorPlugin is not yet installed!");
        }
        return plugin.getProcessName();
    }

    public static String getPackageName() {
        BatteryMonitorPlugin plugin = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
        if (plugin == null) {
            throw new IllegalStateException("BatteryMonitorPlugin is not yet installed!");
        }
        return plugin.getPackageName();
    }

    public static String stackTraceToString(final StackTraceElement[] arr) {
        if (arr == null) {
            return "";
        }

        ArrayList<StackTraceElement> stacks = new ArrayList<>(arr.length);
        for (StackTraceElement traceElement : arr) {
            String className = traceElement.getClassName();
            // remove unused stacks
            if (className.contains("com.tencent.matrix")
                    || className.contains("java.lang.reflect")
                    || className.contains("$Proxy2")
                    || className.contains("android.os")) {
                continue;
            }

            stacks.add(traceElement);
        }
        // stack still too large
        String pkg = getPackageName();
        if (stacks.size() > DEFAULT_MAX_STACK_LAYER && !TextUtils.isEmpty(pkg)) {
            ListIterator<StackTraceElement> iterator = stacks.listIterator(stacks.size());
            // from backward to forward
            while (iterator.hasPrevious()) {
                StackTraceElement stack = iterator.previous();
                String className = stack.getClassName();
                if (!className.contains(pkg)) {
                    iterator.remove();
                }
                if (stacks.size() <= DEFAULT_MAX_STACK_LAYER) {
                    break;
                }
            }
        }
        StringBuilder sb = new StringBuilder(stacks.size());
        for (StackTraceElement stackTraceElement : stacks) {
            sb.append(stackTraceElement).append('\n');
        }
        return sb.toString();
    }

    public static String getThrowableStack(Throwable throwable) {
        if (throwable == null) {
            return "";
        }
        return stackTraceToString(throwable.getStackTrace());
    }

    public static long getUTCTriggerAtMillis(final long triggerAtMillis, final int type) {
        if (type == AlarmManager.RTC || type == AlarmManager.RTC_WAKEUP) {
            return triggerAtMillis;
        }

        return triggerAtMillis + System.currentTimeMillis() - SystemClock.elapsedRealtime();
    }

    public static String getAlarmTypeString(final int type) {
        String typeStr = null;
        switch (type) {
            case AlarmManager.RTC:
                typeStr = "RTC";
                break;
            case AlarmManager.RTC_WAKEUP:
                typeStr = "RTC_WAKEUP";
                break;
            case AlarmManager.ELAPSED_REALTIME:
                typeStr = "ELAPSED_REALTIME";
                break;
            case AlarmManager.ELAPSED_REALTIME_WAKEUP:
                typeStr = "ELAPSED_REALTIME_WAKEUP";
                break;
            default:
                break;
         }
         return typeStr;
    }

    public static int[] getCpuCurrentFreq() {
        int[] output = new int[getNumCores()];
        for (int i = 0; i < getNumCores(); i++) {
            output[i] = 0;
            String path = "/sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_cur_freq";
            String cat = cat(path);
            if (!TextUtils.isEmpty(cat)) {
                try {
                    //noinspection ConstantConditions
                    output[i] = Integer.parseInt(cat) / 1000;
                } catch (Exception ignored) {
                }
            }
        }
        return output;
    }

    private static int getNumCores() {
        try {
            // Get directory containing CPU info
            File dir = new File("/sys/devices/system/cpu/");
            // Filter to only list the devices we care about
            File[] files = dir.listFiles(new FileFilter() {
                @Override
                public boolean accept(File pathname) {
                    return Pattern.matches("cpu[0-9]+", pathname.getName());
                }
            });
            // Return the number of cores (virtual CPU devices)
            return files.length;
        } catch (Exception ignored) {
            // Default to return 1 core
            return 1;
        }
    }

    @Nullable
    static String cat(String path) {
        if (TextUtils.isEmpty(path)) return null;
        try (RandomAccessFile restrictedFile = new RandomAccessFile(path, "r")) {
            return restrictedFile.readLine();
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "cat file fail");
            return null;
        }
    }

    public static int getBatteryTemperature(Context context) {
        Intent batIntent = context.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (batIntent == null) return 0;
        return batIntent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0);
    }

    public static boolean isDeviceCharging(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            BatteryManager myBatteryManager = (BatteryManager) context.getSystemService(Context.BATTERY_SERVICE);
            if (myBatteryManager != null) {
                return myBatteryManager.isCharging();
            }
        }
        Intent batIntent = context.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (batIntent == null) return false;
        int plugged = batIntent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
        return plugged == BatteryManager.BATTERY_PLUGGED_AC || plugged == BatteryManager.BATTERY_PLUGGED_USB || plugged == BatteryManager.BATTERY_PLUGGED_WIRELESS;
    }

    public static boolean isDeviceScreenOn(Context context) {
        try {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            if (pm != null) {
                return Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH ? pm.isInteractive() : pm .isScreenOn();
            }
        } catch (Exception ignored) {
        }
        return false;
    }

    public static boolean isDeviceOnPowerSave(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            try {
                PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
                if (pm != null) {
                    return pm.isPowerSaveMode();
                }
            } catch (Exception ignored) {
            }
        }
        return false;
    }

    public static boolean hasForegroundService(Context context) {
        try {
            ActivityManager am = (ActivityManager) context.getSystemService(ACTIVITY_SERVICE);
            if (am != null) {
                List<ActivityManager.RunningServiceInfo> runningServices = am.getRunningServices(Integer.MAX_VALUE);
                for (ActivityManager.RunningServiceInfo runningServiceInfo : runningServices) {
                    if (!TextUtils.isEmpty(runningServiceInfo.process)
                            && runningServiceInfo.process.startsWith(context.getPackageName())) {
                        if (runningServiceInfo.foreground) {
                            return true;
                        }
                    }
                }
            }
        } catch (Throwable ignored) {
        }
        return false;
    }

    public static List<ActivityManager.RunningServiceInfo> listForegroundServices(Context context) {
        List<ActivityManager.RunningServiceInfo> list = Collections.emptyList();
        try {
            ActivityManager am = (ActivityManager) context.getSystemService(ACTIVITY_SERVICE);
            if (am != null) {
                List<ActivityManager.RunningServiceInfo> runningServices = am.getRunningServices(Integer.MAX_VALUE);
                for (ActivityManager.RunningServiceInfo runningServiceInfo : runningServices) {
                    if (!TextUtils.isEmpty(runningServiceInfo.process)
                            && runningServiceInfo.process.startsWith(context.getPackageName())) {
                        if (runningServiceInfo.foreground) {
                            if (list.isEmpty()) {
                                list = new ArrayList<>();
                            }
                            list.add(runningServiceInfo);
                        }
                    }
                }
            }
        } catch (Throwable ignored) {
        }
        return list;
    }


    @SuppressWarnings("SpellCheckingInspection")
    public static class ProcStatInfo {
        private static final byte[] sBuffer = new byte[128];

        public String comm = "";
        public long utime = -1;
        public long stime = -1;
        public long cutime = -1;
        public long cstime = -1;

        ProcStatInfo() {}

        @Nullable
        public static ProcStatInfo parseJiffies(String path) {
            try {
                return parseJiffiesInfoWithBufferForPath(path, sBuffer);
            } catch (Exception e) {
                MatrixLog.printErrStackTrace(TAG, e, "#parseJiffies fail");
                return null;
            }
        }

        public static ProcStatInfo parseJiffiesInfoWithBufferForPath(String path, byte[] buffer) {
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

        @VisibleForTesting
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
                        // seek end symobl of comm: ')'
                        // noinspection StatementWithEmptyBody
                        for (; i < statBytes && ')' != statBuffer[i]; i++, window++) ;
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
                        spaceIdx = 2;
                        break;
                    }

                    case 14: { // utime
                        int readIdx = i, window = 0;
                        // seek next space
                        // noinspection StatementWithEmptyBody
                        for (; i < statBytes && !Character.isSpaceChar(statBuffer[i]); i++, window++) ;
                        String num = new String(statBuffer, readIdx, window);
                        stat.utime = MatrixUtil.parseLong(num, 0);
                        break;
                    }
                    case 15: { // stime
                        int readIdx = i, window = 0;
                        // seek next space
                        // noinspection StatementWithEmptyBody
                        for (; i < statBytes && !Character.isSpaceChar(statBuffer[i]); i++, window++) ;
                        String num = new String(statBuffer, readIdx, window);
                        stat.stime = MatrixUtil.parseLong(num, 0);
                        break;
                    }
                    case 16: { // cutime
                        int readIdx = i, window = 0;
                        // seek next space
                        // noinspection StatementWithEmptyBody
                        for (; i < statBytes && !Character.isSpaceChar(statBuffer[i]); i++, window++) ;
                        String num = new String(statBuffer, readIdx, window);
                        stat.cutime = MatrixUtil.parseLong(num, 0);
                        break;
                    }
                    case 17: { // cstime
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
            return stat;
        }
    }
}
