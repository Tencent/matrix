package com.tencent.matrix.batterycanary;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore.Callback;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;

/**
 * Matrix Battery Canary Plugin Facades.
 *
 * @author Kaede
 * @since 2021/1/27
 */
final public class BatteryCanary {

    @Nullable
    public static <T extends MonitorFeature> T getMonitorFeature(Class<T> clazz) {
        if (Matrix.isInstalled()) {
            BatteryMonitorPlugin plugin = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
            if (plugin != null) {
                return plugin.core().getMonitorFeature(clazz);
            }
        }
        return null;
    }

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

    public static void addBatteryStateListener(BatteryEventDelegate.Listener listener) {
        if (listener != null) {
            if (BatteryEventDelegate.isInit()) {
                BatteryEventDelegate.getInstance().addListener(listener);
            }
        }
    }

    public static void removeBatteryStateListener(BatteryEventDelegate.Listener listener) {
        if (listener != null) {
            if (BatteryEventDelegate.isInit()) {
                BatteryEventDelegate.getInstance().removeListener(listener);
            }
        }
    }
}
