package com.tencent.matrix.batterycanary.monitor;

import android.support.annotation.NonNull;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;

import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;

/**
 * @author Kaede
 * @since 2020/10/27
 */
@SuppressWarnings("NotNullFieldNotInitialized")
public class BatteryMonitorConfig {
    @NonNull
    public BatteryMonitorCallback callback = new BatteryMonitorCallback.BatteryPrinter();
    public long wakelockTimeout;
    public long greyTime;
    public long foregroundLoopCheckTime;
    public boolean isEnableCheckForeground = false;
    public boolean disableAppForegroundNotifyByMatrix = false;
    public LinkedList<MonitorFeature> plugins = new LinkedList<>();

    public static class Builder {
        private final BatteryMonitorConfig config = new BatteryMonitorConfig();

        public Builder setCallback(BatteryMonitorCallback callback) {
            config.callback = callback;
            return this;
        }

        public Builder wakelockTimeout(long timeout) {
            config.wakelockTimeout = timeout;
            return this;
        }

        public Builder greyJiffiesTime(long time) {
            config.greyTime = time;
            return this;
        }

        public Builder isEnableCheckForeground(boolean isEnable) {
            config.isEnableCheckForeground = isEnable;
            return this;
        }


        public Builder foregroundLoopCheckTime(long time) {
            config.foregroundLoopCheckTime = time;
            return this;
        }

        public Builder enable(Class<? extends MonitorFeature> pluginClass) {
            try {
                config.plugins.add(pluginClass.newInstance());
            } catch (Exception ignored) {
            }
            return this;
        }

        public Builder disableAppForegroundNotifyByMatrix(boolean enable) {
            config.disableAppForegroundNotifyByMatrix = enable;
            return this;
        }

        public BatteryMonitorConfig build() {
            Collections.sort(config.plugins, new Comparator<MonitorFeature>() {
                @Override
                public int compare(MonitorFeature o1, MonitorFeature o2) {
                    return Integer.compare(o2.weight(), o1.weight());
                }
            });
            return config;
        }
    }
}
