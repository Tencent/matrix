package sample.tencent.matrix.battery;

import android.annotation.SuppressLint;
import android.content.Context;

import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;

/**
 * @author Kaede
 * @since 2021/4/23
 */
@SuppressLint("LongLogTag")
public final class BatteryCanarySimpleInitHelper {
    private static final String TAG = "Matrix.BatteryCanaryDemo";

    public static BatteryMonitorPlugin createMonitor(Context context) {
        return new BatteryMonitorPlugin(new BatteryMonitorConfig.Builder()
                // Thread Activities Monitor
                .enable(JiffiesMonitorFeature.class)
                .build());
    }
}
