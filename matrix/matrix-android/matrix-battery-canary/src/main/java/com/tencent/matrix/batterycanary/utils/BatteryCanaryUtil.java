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

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.AlarmManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;
import android.os.SystemClock;
import android.text.TextUtils;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.io.FileFilter;
import java.io.RandomAccessFile;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.ListIterator;
import java.util.regex.Pattern;

import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;

import static android.content.Context.ACTIVITY_SERVICE;
import static com.tencent.matrix.batterycanary.monitor.AppStats.APP_STAT_BACKGROUND;
import static com.tencent.matrix.batterycanary.monitor.AppStats.APP_STAT_FOREGROUND;
import static com.tencent.matrix.batterycanary.monitor.AppStats.APP_STAT_FOREGROUND_SERVICE;
import static com.tencent.matrix.batterycanary.monitor.AppStats.DEV_STAT_CHARGING;
import static com.tencent.matrix.batterycanary.monitor.AppStats.DEV_STAT_DOZE_MODE_OFF;
import static com.tencent.matrix.batterycanary.monitor.AppStats.DEV_STAT_DOZE_MODE_ON;
import static com.tencent.matrix.batterycanary.monitor.AppStats.DEV_STAT_SAVE_POWER_MODE_OFF;
import static com.tencent.matrix.batterycanary.monitor.AppStats.DEV_STAT_SAVE_POWER_MODE_ON;
import static com.tencent.matrix.batterycanary.monitor.AppStats.DEV_STAT_SCREEN_OFF;
import static com.tencent.matrix.batterycanary.monitor.AppStats.DEV_STAT_SCREEN_ON;
import static com.tencent.matrix.batterycanary.monitor.AppStats.DEV_STAT_UN_CHARGING;

/**
 * @author liyongjie
 * Created by liyongjie on 2017/8/14.
 */
@RestrictTo(RestrictTo.Scope.LIBRARY)
public final class BatteryCanaryUtil {
    private static final String TAG = "Matrix.battery.Utils";
    private static final int DEFAULT_MAX_STACK_LAYER = 10;
    private static final int DEFAULT_AMS_CACHE_MILLIS = 5 * 1000;

    public static final int ONE_MIN = 60 * 1000;
    public static final int ONE_HOR = 60 * 60 * 1000;
    public static final int JIFFY_HZ = 100; // @Os.sysconf(OsConstants._SC_CLK_TCK)
    public static final int JIFFY_MILLIS = 1000 / JIFFY_HZ;

    public interface Proxy {
        String getProcessName();
        String getPackageName();
        int getBatteryTemperature(Context context);
        @AppStats.AppStatusDef int getAppStat(Context context, boolean isForeground);
        @AppStats.DevStatusDef int getDevStat(Context context);
        void updateAppStat(int value);
        void updateDevStat(int value);

        final class ExpireRef {
            final int value;
            final long aliveMillis;
            final long lastMillis;

            ExpireRef(int value, long aliveMillis) {
                this.value = value;
                this.aliveMillis = aliveMillis;
                this.lastMillis = SystemClock.uptimeMillis();
            }

            boolean isExpired() {
                return (SystemClock.uptimeMillis() - lastMillis) >= aliveMillis;
            }
        }
    }

    static Proxy sCacheStub = new Proxy() {
        private String mProcessName;
        private String mPackageName;
        private ExpireRef mBatteryTemp;
        private ExpireRef mLastAppStat;
        private ExpireRef mLastDevStat;

        @Override
        public String getProcessName() {
            if (!TextUtils.isEmpty(mProcessName)) {
                return mProcessName;
            }
            BatteryMonitorPlugin plugin = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (plugin == null) {
                throw new IllegalStateException("BatteryMonitorPlugin is not yet installed!");
            }
            mProcessName = plugin.getProcessName();
            return mProcessName;
        }

        @Override
        public String getPackageName() {
            if (!TextUtils.isEmpty(mPackageName)) {
                return mPackageName;
            }
            BatteryMonitorPlugin plugin = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (plugin == null) {
                throw new IllegalStateException("BatteryMonitorPlugin is not yet installed!");
            }
            mPackageName = plugin.getPackageName();
            return mPackageName;
        }

        @Override
        public int getBatteryTemperature(Context context) {
            if (mBatteryTemp != null && !mBatteryTemp.isExpired()) {
                return mBatteryTemp.value;
            }
            int tmp = getBatteryTemperatureImmediately(context);
            mBatteryTemp = new ExpireRef(tmp, DEFAULT_AMS_CACHE_MILLIS);
            return mBatteryTemp.value;
        }

        @Override
        public int getAppStat(Context context, boolean isForeground) {
            if (isForeground) return APP_STAT_FOREGROUND; // 前台
            if (mLastAppStat != null && !mLastAppStat.isExpired()) {
                return mLastAppStat.value;
            }
            int value = getAppStatImmediately(context, false);
            mLastAppStat = new ExpireRef(value, DEFAULT_AMS_CACHE_MILLIS);
            return mLastAppStat.value;
        }

        @Override
        public int getDevStat(Context context) {
            if (mLastDevStat != null && !mLastDevStat.isExpired()) {
                return mLastDevStat.value;
            }
            int value = getDeviceStatImmediately(context);
            mLastDevStat = new ExpireRef(value, DEFAULT_AMS_CACHE_MILLIS);
            return mLastDevStat.value;
        }

        @Override
        public void updateAppStat(int value) {
            synchronized (this) {
                mLastAppStat = new ExpireRef(value, DEFAULT_AMS_CACHE_MILLIS);
            }
        }

        @Override
        public void updateDevStat(int value) {
            synchronized (this) {
                mLastDevStat = new ExpireRef(value, DEFAULT_AMS_CACHE_MILLIS);
            }
        }
    };

    public static void setProxy(Proxy stub) {
        sCacheStub = stub;
    }

    public static Proxy getProxy() {
        return sCacheStub;
    }

    public static String getProcessName() {
        return sCacheStub.getProcessName();
    }

    public static String getPackageName() {
        return sCacheStub.getPackageName();
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
        StringBuilder sb = new StringBuilder();
        for (StackTraceElement traceElement : stacks) {
            sb.append("\n").append("at ").append(traceElement);
        }
        return sb.length() > 0 ? "Matrix" + sb.toString() : "";
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
        int[] output = new int[getCpuCoreNum()];
        for (int i = 0; i < getCpuCoreNum(); i++) {
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

    public static int getCpuCoreNum() {
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
    public static String cat(String path) {
        if (TextUtils.isEmpty(path)) return null;
        try (RandomAccessFile restrictedFile = new RandomAccessFile(path, "r")) {
            return restrictedFile.readLine();
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "cat file fail");
            return null;
        }
    }

    public static int getBatteryTemperature(Context context) {
        return sCacheStub.getBatteryTemperature(context);
    }

    public static int getBatteryTemperatureImmediately(Context context) {
        Intent batIntent = getBatteryStickyIntent(context);
        if (batIntent == null) return 0;
        return batIntent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0);
    }

    @AppStats.AppStatusDef
    public static int getAppStat(Context context, boolean isForeground) {
        return sCacheStub.getAppStat(context, isForeground);
    }

    @AppStats.AppStatusDef
    public static int getAppStatImmediately(Context context, boolean isForeground) {
        if (isForeground) return APP_STAT_FOREGROUND; // 前台
        if (hasForegroundService(context)) {
            return APP_STAT_FOREGROUND_SERVICE; // 后台（有前台服务）
        }
        return APP_STAT_BACKGROUND; // 后台
    }

    @AppStats.DevStatusDef
    public static int getDeviceStat(Context context) {
        return sCacheStub.getDevStat(context);
    }

    @AppStats.DevStatusDef
    public static int getDeviceStatImmediately(Context context) {
        if (isDeviceCharging(context)) {
            return DEV_STAT_CHARGING; // 充电中
        }

        // 未充电状态细分:
        if (!isDeviceScreenOn(context)) {
            return DEV_STAT_SCREEN_OFF; // 息屏
        }
        if (isDeviceOnPowerSave(context)) {
            return DEV_STAT_SAVE_POWER_MODE_ON; // 省电模式开启
        }
        return DEV_STAT_UN_CHARGING;
    }

    public static String convertAppStat(@AppStats.AppStatusDef int appStat) {
        switch (appStat) {
            case APP_STAT_FOREGROUND:
                return "fg";
            case APP_STAT_BACKGROUND:
                return "bg";
            case APP_STAT_FOREGROUND_SERVICE:
                return "fgSrv";
            default:
                return "unknown";
        }
    }

    public static String convertDevStat(@AppStats.DevStatusDef int devStat) {
        switch (devStat) {
            case DEV_STAT_CHARGING:
                return "charging";
            case DEV_STAT_UN_CHARGING:
                return "non_charge";
            case DEV_STAT_SCREEN_ON:
                return "screen_on";
            case DEV_STAT_SCREEN_OFF:
                return "screen_off";
            case DEV_STAT_DOZE_MODE_ON:
                return "doze_on";
            case DEV_STAT_DOZE_MODE_OFF:
                return "doze_off";
            case DEV_STAT_SAVE_POWER_MODE_ON:
                return "standby_on";
            case DEV_STAT_SAVE_POWER_MODE_OFF:
                return "standby_off";
            default:
                return "unknown";
        }
    }

    public static boolean isDeviceChargingV1(Context context) {
        Intent batIntent = getBatteryStickyIntent(context);
        if (batIntent == null) return false;
        int status = batIntent.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        return (status == BatteryManager.BATTERY_STATUS_CHARGING) || (status == BatteryManager.BATTERY_STATUS_FULL);
    }

    public static boolean isDeviceChargingV2(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            BatteryManager myBatteryManager = (BatteryManager) context.getSystemService(Context.BATTERY_SERVICE);
            if (myBatteryManager != null) {
                return myBatteryManager.isCharging();
            }
        }
        try {
            Intent batIntent = getBatteryStickyIntent(context);
            if (batIntent == null) return false;
            int plugged = batIntent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
            return plugged == BatteryManager.BATTERY_PLUGGED_AC || plugged == BatteryManager.BATTERY_PLUGGED_USB || plugged == BatteryManager.BATTERY_PLUGGED_WIRELESS;
        } catch (Throwable ignored) {
            return false;
        }
    }

    public static boolean isDeviceCharging(Context context) {
        return isDeviceChargingV1(context);
    }

    public static boolean isDeviceScreenOn(Context context) {
        try {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            if (pm != null) {
                return Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH ? pm.isInteractive() : pm.isScreenOn();
            }
        } catch (Exception ignored) {
        }
        return false;
    }

    /**
     * System Doze Mode
     */
    public static boolean isDeviceOnIdleMode(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            try {
                PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
                if (pm != null) {
                    return pm.isDeviceIdleMode();
                }
            } catch (Exception ignored) {
            }
        }
        return false;
    }

    /**
     * App Standby Mode
     */
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

    @Nullable
    public static Intent getBatteryStickyIntent(Context context) {
        try {
            return context.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        } catch (Exception e) {
            MatrixLog.w(TAG, "get ACTION_BATTERY_CHANGED failed: " + e.getMessage());
            return null;
        }
    }

    public static boolean isLowBattery(Context context) {
        Intent batIntent = getBatteryStickyIntent(context);
        if (batIntent != null) {
            batIntent.getBooleanExtra(Intent.ACTION_BATTERY_LOW, false);
        }
        return false;
    }

    public static int getBatteryPercentage(Context context) {
        Intent batIntent = getBatteryStickyIntent(context);
        if (batIntent != null) {
            int level = batIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
            int scale = batIntent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
            if (scale > 0) {
                return level * 100 / scale;
            }
        }
        return -1;
    }

    @SuppressWarnings("ConstantConditions")
    @SuppressLint("PrivateApi")
    public static int getBatteryCapacity(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            BatteryManager mBatteryManager = (BatteryManager) context.getSystemService(Context.BATTERY_SERVICE);
            int chargeCounter = mBatteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER);
            int capacity = mBatteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY);
            if (chargeCounter > 0 && capacity > 0) {
                return (int) (((chargeCounter / (float) capacity) * 100) / 1000);
            }
        }

        if (PowerProfile.getInstance() != null) {
            return (int) PowerProfile.getInstance().getBatteryCapacity();
        }

        try {
            Class<?> profileClass = Class.forName("com.android.internal.os.PowerProfile");
            Object profileObject = profileClass.getConstructor(Context.class).newInstance(context);
            Method method;
            try {
                method = profileClass.getMethod("getAveragePower", String.class);
                return (int) method.invoke(profileObject, "battery.capacity");
            } catch (Throwable e) {
                MatrixLog.w(TAG, "get PowerProfile failed: " + e.getMessage());
            }
            method = profileClass.getMethod("getBatteryCapacity");
            return (int) method.invoke(profileObject);
        } catch (Throwable e) {
            MatrixLog.w(TAG, "get PowerProfile failed: " + e.getMessage());
        }
        return -1;
    }

    public static boolean hasForegroundService(Context context) {
        try {
            ActivityManager am = (ActivityManager) context.getSystemService(ACTIVITY_SERVICE);
            if (am != null) {
                List<ActivityManager.RunningServiceInfo> runningServices = am.getRunningServices(Integer.MAX_VALUE);
                if (runningServices != null) {
                    for (ActivityManager.RunningServiceInfo runningServiceInfo : runningServices) {
                        if (!TextUtils.isEmpty(runningServiceInfo.process)
                                && runningServiceInfo.process.startsWith(context.getPackageName())) {
                            if (runningServiceInfo.foreground) {
                                return true;
                            }
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
                if (runningServices != null) {
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
            }
        } catch (Throwable ignored) {
        }
        return list;
    }

    public static String polishStack(String stackTrace, String startSymbol) {
        List<String> stacks = new ArrayList<>();
        String[] splits = stackTrace.split("\n\t");
        boolean startFilter = TextUtils.isEmpty(startSymbol);
        for (String line : splits) {
            if (!TextUtils.isEmpty(line)) {
                if (!startFilter && line.startsWith(startSymbol)) {
                    startFilter = true;
                }
                if (startFilter) {
                    stacks.add(line.trim());
                }
            }
        }
        return TextUtils.join(";", stacks.subList(0, Math.min(5, stacks.size())));
    }

    public static long computeAvgByMinute(long input, long millis) {
        if (millis < ONE_MIN) {
            long scale = 100L;
            long divideBase = Math.max(1, (millis * scale) / ONE_MIN);
            return (input / divideBase) * scale;
        } else {
            return input / Math.max(1, (millis) / ONE_MIN);
        }
    }
}
