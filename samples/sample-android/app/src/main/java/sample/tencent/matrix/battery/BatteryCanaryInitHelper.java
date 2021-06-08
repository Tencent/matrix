package sample.tencent.matrix.battery;

import android.util.Log;

import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.AppStats;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.feature.AlarmMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.AppStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.BlueToothMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LocationMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.LooperTaskMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.TrafficMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WakeLockMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.WifiMonitorFeature;

import java.util.concurrent.Callable;

import androidx.annotation.NonNull;

/**
 * @author Kaede
 * @since 2021/4/23
 */
public final class BatteryCanaryInitHelper {

    public static BatteryMonitorPlugin createMonitor() {
        BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
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
                .setCallback(new BatteryMonitorCallback.BatteryPrinter() {
                    @Override
                    public void onWakeLockTimeout(WakeLockMonitorFeature.WakeLockTrace.WakeLockRecord record, long backgroundMillis) {
                        // WakeLock acquired too long
                    }
                    @Override
                    protected void onCanaryDump(AppStats appStats) {
                        // Dump battery stats data periodically
                        long statMinute = appStats.getMinute();
                        boolean foreground = appStats.isForeground();
                        boolean charging = appStats.isCharging();
                        super.onCanaryDump(appStats);
                    }
                    @Override
                    protected void onReportJiffies(@NonNull MonitorFeature.Snapshot.Delta<JiffiesMonitorFeature.JiffiesSnapshot> delta) {
                        // Report all threads jiffies consumed during the statMinute time
                    }
                    @Override
                    protected void onReportAlarm(@NonNull MonitorFeature.Snapshot.Delta<AlarmMonitorFeature.AlarmSnapshot> delta) {
                        // Report all alarm set during the statMinute time
                    }
                    @Override
                    protected void onReportWakeLock(@NonNull MonitorFeature.Snapshot.Delta<WakeLockMonitorFeature.WakeLockSnapshot> delta) {
                        // Report all wakelock acquired during the statMinute time
                    }
                    @Override
                    protected void onReportBlueTooth(@NonNull MonitorFeature.Snapshot.Delta<BlueToothMonitorFeature.BlueToothSnapshot> delta) {
                        // Report all bluetooth scanned during the statMinute time
                    }
                    @Override
                    protected void onReportWifi(@NonNull MonitorFeature.Snapshot.Delta<WifiMonitorFeature.WifiSnapshot> delta) {
                        // Report all wifi scanned during the statMinute time
                    }
                    @Override
                    protected void onReportLocation(@NonNull MonitorFeature.Snapshot.Delta<LocationMonitorFeature.LocationSnapshot> delta) {
                        // Report all gps scanned during the statMinute time
                    }

                    // @Override
                    // public void onNotify(String text, long bgMillis) {
                    //     Log.w("BatteryCanaryInitHelper", "Notification with illegal text found: " + text);
                    // }

                }).build();

        return new BatteryMonitorPlugin(config);
    }
}
