package com.tencent.matrix.batterycanary.monitor;

import android.support.annotation.NonNull;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * @author Kaede
 * @since 2020/10/27
 */
public class BatteryMonitorConfig {
    public static final long DEF_WAKELOCK_TIMEOUT = 2 * 60 * 1000L; // 2min
    public static final long DEF_JIFFIES_DELAY = 30 * 1000L; // 30s
    public static final long DEF_FOREGROUND_SCHEDULE_TIME = 10 * 60 * 1000L; // 10min

    @NonNull
    public BatteryMonitorCallback callback = new BatteryMonitorCallback.BatteryPrinter();
    public long wakelockTimeout = DEF_WAKELOCK_TIMEOUT;
    public long greyTime = DEF_JIFFIES_DELAY;
    public long foregroundLoopCheckTime = DEF_FOREGROUND_SCHEDULE_TIME;
    public boolean isForegroundModeEnabled = true;
    public boolean isBuiltinForegroundNotifyEnabled = true;
    public final List<MonitorFeature> features = new ArrayList<>(3);

    private BatteryMonitorConfig() {}

    /**
     * FIXME: suitable builder needed
     */
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

        public Builder enableForegroundMode(boolean isEnable) {
            config.isForegroundModeEnabled = isEnable;
            return this;
        }


        public Builder foregroundLoopCheckTime(long time) {
            config.foregroundLoopCheckTime = time;
            return this;
        }

        public Builder enable(Class<? extends MonitorFeature> pluginClass) {
            try {
                config.features.add(pluginClass.newInstance());
            } catch (Exception ignored) {
            }
            return this;
        }

        public Builder enableBuiltinForegroundNotify(boolean enable) {
            config.isBuiltinForegroundNotifyEnabled = enable;
            return this;
        }

        public BatteryMonitorConfig build() {
            Collections.sort(config.features, new Comparator<MonitorFeature>() {
                @Override
                public int compare(MonitorFeature o1, MonitorFeature o2) {
                    return Integer.compare(o2.weight(), o1.weight());
                }
            });
            return config;
        }
    }
}
