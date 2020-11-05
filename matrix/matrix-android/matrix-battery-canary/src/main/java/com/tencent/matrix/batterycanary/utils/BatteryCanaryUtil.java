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

import android.app.AlarmManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;
import android.text.TextUtils;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.io.FileFilter;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.ListIterator;
import java.util.regex.Pattern;

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
    private static String cat(String path) {
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
}
