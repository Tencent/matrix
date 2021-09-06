package sample.tencent.matrix.battery;

import android.annotation.SuppressLint;
import android.util.Log;

import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback.BatteryPrinter;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback.BatteryPrinter.Printer;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature.AlarmSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature.BlueToothSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature.LocationSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature.WifiSnapshot;

import java.util.concurrent.Callable;

import androidx.annotation.NonNull;

/**
 * @author Kaede
 * @since 2021/4/23
 */
@SuppressLint("LongLogTag")
public final class BatteryCanaryInitHelper {
    private static final String TAG = "Matrix.BatteryCanaryDemo";

    static BatteryMonitorConfig sBatteryConfig;

    public static BatteryMonitorPlugin createMonitor() {
        if (sBatteryConfig != null) {
            throw new IllegalStateException("Duplicated init!");
        }

        sBatteryConfig = new BatteryMonitorConfig.Builder()
                // Thread Activities Monitor
                .enable(JiffiesMonitorFeature.class)
                .enableStatPidProc(true)
                .greyJiffiesTime(3 * 1000L)
                .enableBackgroundMode(false)
                .backgroundLoopCheckTime(30 * 60 * 1000L)
                .enableForegroundMode(true)
                .foregroundLoopCheckTime(20 * 60 * 1000L)
                .setBgThreadWatchingLimit(5000)
                .setBgThreadWatchingLimit(8000)

                // App & Device Status Monitor For Better Invalid Battery Activities Configure
                .setOverHeatCount(1024)
                .enable(DeviceStatMonitorFeature.class)
                .enable(AppStatMonitorFeature.class)
                .setSceneSupplier(new Callable<String>() {
                    @Override
                    public String call() {
                        return "Current AppScene";
                    }
                })

                // AMS Activities Monitor:
                // alarm/wakelock watch
                .enableAmsHook(true)
                .enable(AlarmMonitorFeature.class)
                .enable(WakeLockMonitorFeature.class)
                .wakelockTimeout(2 * 60 * 1000L)
                .wakelockWarnCount(3)
                .addWakeLockWhiteList("Ignore WakeLock TAG1")
                .addWakeLockWhiteList("Ignore WakeLock TAG2")
                // scanning watch (wifi/gps/bluetooth)
                .enable(WifiMonitorFeature.class)
                .enable(LocationMonitorFeature.class)
                .enable(BlueToothMonitorFeature.class)
                // .enable(NotificationMonitorFeature.class)

                // Lab Feature:
                // network monitor
                // looper task monitor
                .enable(TrafficMonitorFeature.class)
                .enable(LooperTaskMonitorFeature.class)
                .addLooperWatchList("main")
                .useThreadClock(false)
                .enableAggressive(true)

                // Monitor Callback
                .setCallback(new BatteryStatsListener())
                .build();

        return new BatteryMonitorPlugin(sBatteryConfig);
    }

    public static BatteryMonitorConfig getConfig() {
        if (sBatteryConfig == null) {
            throw new IllegalStateException("BatteryCanary has not yet init!");
        }
        return sBatteryConfig;
    }

    public static String convertStatsToReadFriendlyText(Delta<?> sessionDelta) {
        return new BatteryStatsListener().convertStatsToReadFriendlyText(sessionDelta);
    }


    public static class BatteryStatsListener extends BatteryPrinter {

        /**
         * Just for test, you should handle the battery stats yourself within {@link BatteryMonitorCallback}
         */
        public String convertStatsToReadFriendlyText(Delta<?> delta) {
            Printer printer = new Printer();
            printer.writeTitle();
            printer.append("| Read Friendly Battery Stats").append("\n");
            onWritingSectionContent(delta, AppStats.current(delta.dlt.time), printer);
            printer.writeEnding();
            return printer.toString();
        }

        @Override
        public void onWakeLockTimeout(WakeLockRecord record, long backgroundMillis) {
            // WakeLock acquired too long
        }
        @Override
        protected void onCanaryDump(AppStats appStats) {
            // Dump battery stats data periodically
            long statMinute = appStats.getMinute();
            boolean foreground = appStats.isForeground();
            boolean charging = appStats.isCharging();
            Log.w(TAG, "onDumpBatteryStatsReport, statMinute " + appStats.getMinute()
                    + ", foreground = " + foreground
                    + ", charging = " + charging);
            super.onCanaryDump(appStats);
        }
        @Override
        protected void onReportJiffies(@NonNull Delta<JiffiesSnapshot> delta) {
            // Report all threads jiffies consumed during the statMinute time
        }
        @Override
        protected void onReportAlarm(@NonNull Delta<AlarmSnapshot> delta) {
            // Report all alarm set during the statMinute time
        }
        @Override
        protected void onReportWakeLock(@NonNull Delta<WakeLockSnapshot> delta) {
            // Report all wakelock acquired during the statMinute time
        }
        @Override
        protected void onReportBlueTooth(@NonNull Delta<BlueToothSnapshot> delta) {
            // Report all bluetooth scanned during the statMinute time
        }
        @Override
        protected void onReportWifi(@NonNull Delta<WifiSnapshot> delta) {
            // Report all wifi scanned during the statMinute time
        }
        @Override
        protected void onReportLocation(@NonNull Delta<LocationSnapshot> delta) {
            // Report all gps scanned during the statMinute time
        }

        // @Override
        // public void onNotify(String text, long bgMillis) {
        //     Log.w("BatteryCanaryInitHelper", "Notification with illegal text found: " + text);
        // }
    }
}
