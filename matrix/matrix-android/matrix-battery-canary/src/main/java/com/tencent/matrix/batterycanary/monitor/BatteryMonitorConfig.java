package com.tencent.matrix.batterycanary.monitor;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.tencent.matrix.batterycanary.BuildConfig;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * @author Kaede
 * @since 2020/10/27
 */
@SuppressWarnings({"SpellCheckingInspection"})
public class BatteryMonitorConfig {
    public static final long DEF_WAKELOCK_TIMEOUT = 2 * 60 * 1000L; // 2min
    public static final long DEF_JIFFIES_DELAY = 30 * 1000L; // 30s
    public static final long DEF_FOREGROUND_SCHEDULE_TIME = 10 * 60 * 1000L; // 10min

    @NonNull
    public BatteryMonitorCallback callback = new BatteryMonitorCallback.BatteryPrinter();
    @Nullable
    public Callable<String> onSceneSupplier;

    public long wakelockTimeout = DEF_WAKELOCK_TIMEOUT;
    public long greyTime = DEF_JIFFIES_DELAY;
    public long foregroundLoopCheckTime = DEF_FOREGROUND_SCHEDULE_TIME;
    public int overHeatCount = 1024;
    public boolean isForegroundModeEnabled = true;
    public boolean isBuiltinForegroundNotifyEnabled = true;
    public boolean isStatAsSample = BuildConfig.DEBUG;
    public boolean isStatPidProc = BuildConfig.DEBUG;
    public List<String> tagWhiteList = Collections.emptyList();
    public List<String> tagBlackList = Collections.emptyList();
    public final List<MonitorFeature> features = new ArrayList<>(3);

    private BatteryMonitorConfig() {}

    @Override
    public String toString() {
        return "BatteryMonitorConfig{" +
                ", wakelockTimeout=" + wakelockTimeout +
                ", greyTime=" + greyTime +
                ", foregroundLoopCheckTime=" + foregroundLoopCheckTime +
                ", overHeatCount=" + overHeatCount +
                ", isForegroundModeEnabled=" + isForegroundModeEnabled +
                ", isBuiltinForegroundNotifyEnabled=" + isBuiltinForegroundNotifyEnabled +
                ", isStatAsSample=" + isStatAsSample +
                ", tagWhiteList=" + tagWhiteList +
                ", features=" + features +
                '}';
    }

    /**
     * FIXME: suitable builder needed
     */
    public static class Builder {
        private final BatteryMonitorConfig config = new BatteryMonitorConfig();

        public Builder setCallback(BatteryMonitorCallback callback) {
            config.callback = callback;
            return this;
        }

        public Builder setSceneSupplier(Callable<String> block) {
            config.onSceneSupplier = block;
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

        public Builder enableStatAsSample(boolean isEnable) {
            config.isStatAsSample = isEnable;
            return this;
        }

        public Builder enableStatPidProc(boolean isEnable) {
            config.isStatPidProc = isEnable;
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

        public Builder addWakeLockWhiteList(String tag) {
            if (config.tagWhiteList == Collections.EMPTY_LIST) {
                config.tagWhiteList = new ArrayList<>();
            }
            config.tagWhiteList.add(tag);
            return this;
        }

        public Builder addWakeLockBlackList(String tag) {
            if (config.tagBlackList == Collections.EMPTY_LIST) {
                config.tagBlackList = new ArrayList<>();
            }
            config.tagBlackList.add(tag);
            return this;
        }

        public Builder setOverHeatCount(int count) {
            if (count >= 10) {
                config.overHeatCount = count;
            }
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
