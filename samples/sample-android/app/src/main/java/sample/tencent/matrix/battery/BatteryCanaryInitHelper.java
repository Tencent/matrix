package sample.tencent.matrix.battery;

import android.annotation.SuppressLint;
import android.content.Context;
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
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.monitor.feature.CpuStatFeature;
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
import com.tencent.matrix.batterycanary.stats.BatteryRecorder;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.mmkv.MMKV;

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

    public static BatteryMonitorPlugin createMonitor(Context context) {
        if (sBatteryConfig != null) {
            throw new IllegalStateException("Duplicated init!");
        }

        // Init MMKV only when BatteryStatsFeature is & MMKVRecorder is enabled.
        MMKV.initialize(context);

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

                // CPU Stats
                .enable(CpuStatFeature.class)

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

                // BatteryStats
                .enable(BatteryStatsFeature.class)
                .setRecorder(new BatteryRecorder.MMKVRecorder(MMKV.defaultMMKV()))

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


    public static class BatteryStatsListener extends BatteryPrinter {
        @Override
        protected void onCanaryDump(CompositeMonitors monitors) {
            monitors.getAppStats(new Consumer<AppStats>() {
                @Override
                public void accept(AppStats appStats) {
                    // Dump battery stats data periodically
                    long statMinute = appStats.getMinute();
                    boolean foreground = appStats.isForeground();
                    boolean charging = appStats.isCharging();
                    Log.w(TAG, "onDumpBatteryStatsReport, statMinute " + appStats.getMinute()
                            + ", foreground = " + foreground
                            + ", charging = " + charging);
                }
            });
            super.onCanaryDump(monitors);
        }

        @Override
        protected void onCanaryReport(CompositeMonitors monitors) {
            super.onCanaryReport(monitors);
            // Report all enabled battery canary monitors' data here
        }
    }
}
