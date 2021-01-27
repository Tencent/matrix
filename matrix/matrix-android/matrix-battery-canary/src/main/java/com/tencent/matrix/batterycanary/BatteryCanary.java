package com.tencent.matrix.batterycanary;

import android.support.annotation.NonNull;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore.Callback;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;

/**
 * Matrix Battery Canary Plugin Facades.
 *
 * @author Kaede
 * @since 2021/1/27
 */
final public class BatteryCanary {
    public static void currentJiffies(@NonNull final Callback<JiffiesSnapshot> callback) {
        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin plugin = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (plugin != null) {
                JiffiesMonitorFeature jiffiesFeat = plugin.core().getMonitorFeature(JiffiesMonitorFeature.class);
                if (jiffiesFeat != null) {
                    jiffiesFeat.currentJiffiesSnapshot(callback);
                }
            }
        }
    }
}
