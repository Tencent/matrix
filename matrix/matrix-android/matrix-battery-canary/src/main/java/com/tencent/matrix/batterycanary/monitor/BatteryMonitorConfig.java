package com.tencent.matrix.batterycanary.monitor;

import android.app.ActivityManager;
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
    public static final int DEF_STAMP_OVERHEAT = 200;
    public static final int DEF_WAKELOCK_WARN_COUNT = 30;
    public static final long DEF_WAKELOCK_TIMEOUT = 2 * 60 * 1000L; // 2min
    public static final long DEF_JIFFIES_DELAY = 30 * 1000L; // 30s
    public static final long DEF_FOREGROUND_SCHEDULE_TIME = 20 * 60 * 1000L; // 10min
    public static final long DEF_BACKGROUND_SCHEDULE_TIME = 10 * 60 * 1000L; // 10min

    @NonNull
    public BatteryMonitorCallback callback = new BatteryMonitorCallback.BatteryPrinter();
    @Nullable
    public Callable<String> onSceneSupplier;

    public long wakelockTimeout = DEF_WAKELOCK_TIMEOUT;
    public int wakelockWarnCount = DEF_WAKELOCK_WARN_COUNT;
    public long greyTime = DEF_JIFFIES_DELAY;
    public long foregroundLoopCheckTime = DEF_FOREGROUND_SCHEDULE_TIME;
    public long backgroundLoopCheckTime = DEF_BACKGROUND_SCHEDULE_TIME;
    public int overHeatCount = DEF_STAMP_OVERHEAT;
    public int foregroundServiceLeakLimit = ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;
    public int fgThreadWatchingLimit = 10000;
    public int bgThreadWatchingLimit = 5000;
    public boolean isForegroundModeEnabled = true;
    public boolean isBackgroundModeEnabled = false;
    public boolean isBuiltinForegroundNotifyEnabled = true;
    public boolean isStatAsSample = BuildConfig.DEBUG;
    public boolean isStatPidProc = BuildConfig.DEBUG;
    public boolean isInspectiffiesError = BuildConfig.DEBUG;
    public boolean isAmsHookEnabled = BuildConfig.DEBUG;
    public boolean isAggressiveMode = BuildConfig.DEBUG;
    public boolean isUseThreadClock = BuildConfig.DEBUG;
    public List<String> tagWhiteList = Collections.emptyList();
    public List<String> tagBlackList = Collections.emptyList();
    public List<String> looperWatchList = Collections.emptyList();
    public List<String> threadWatchList = Collections.emptyList();
    public final List<MonitorFeature> features = new ArrayList<>(3);

    private BatteryMonitorConfig() {
    }

    @NonNull
    @Override
    public String toString() {
        return "BatteryMonitorConfig{"
                + "wakelockTimeout=" + wakelockTimeout
                + ", wakelockWarnCount=" + wakelockWarnCount
                + ", greyTime=" + greyTime
                + ", foregroundLoopCheckTime=" + foregroundLoopCheckTime
                + ", backgroundLoopCheckTime=" + backgroundLoopCheckTime
                + ", overHeatCount=" + overHeatCount
                + ", foregroundServiceLeakLimit=" + foregroundServiceLeakLimit
                + ", fgThreadWatchingLimit=" + fgThreadWatchingLimit
                + ", bgThreadWatchingLimit=" + bgThreadWatchingLimit
                + ", isForegroundModeEnabled=" + isForegroundModeEnabled
                + ", isBackgroundModeEnabled=" + isBackgroundModeEnabled
                + ", isBuiltinForegroundNotifyEnabled=" + isBuiltinForegroundNotifyEnabled
                + ", isStatAsSample=" + isStatAsSample
                + ", isStatPidProc=" + isStatPidProc
                + ", isInspectiffiesError=" + isInspectiffiesError
                + ", isAmsHookEnabled=" + isAmsHookEnabled
                + ", isAggressiveMode=" + isAggressiveMode
                + ", isUseThreadClock=" + isUseThreadClock
                + ", tagWhiteList=" + tagWhiteList
                + ", tagBlackList=" + tagBlackList
                + ", looperWatchList=" + looperWatchList
                + ", threadWatchList=" + threadWatchList
                + ", features=" + features
                + '}';
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
            if (timeout > 0) {
                config.wakelockTimeout = timeout;
            }
            return this;
        }

        public Builder wakelockWarnCount(int count) {
            if (count > 0) {
                config.wakelockWarnCount = count;
            }
            return this;
        }

        public Builder greyJiffiesTime(long time) {
            if (time > 0) {
                config.greyTime = time;
            }
            return this;
        }

        public Builder enableForegroundMode(boolean isEnable) {
            config.isForegroundModeEnabled = isEnable;
            return this;
        }

        public Builder enableBackgroundMode(boolean isEnable) {
            config.isBackgroundModeEnabled = isEnable;
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

        public Builder enableInspectJffiesError(boolean isEnable) {
            config.isInspectiffiesError = isEnable;
            return this;
        }

        public Builder enableAmsHook(boolean isEnable) {
            config.isAmsHookEnabled = isEnable;
            return this;
        }

        public Builder enableAggressive(boolean isEnable) {
            config.isAggressiveMode = isEnable;
            return this;
        }

        public Builder useThreadClock(boolean isEnable) {
            config.isUseThreadClock = isEnable;
            return this;
        }

        public Builder foregroundLoopCheckTime(long time) {
            if (time > 0) {
                config.foregroundLoopCheckTime = time;
            }
            return this;
        }

        public Builder backgroundLoopCheckTime(long time) {
            if (time > 0) {
                config.backgroundLoopCheckTime = time;
            }
            return this;
        }

        public Builder foregroundServiceLeakLimit(int importanceLimit) {
            config.foregroundServiceLeakLimit = importanceLimit;
            return this;
        }

        public Builder setFgThreadWatchingLimit(int fgThreadWatchingLimit) {
            if (fgThreadWatchingLimit > 1000) {
                config.fgThreadWatchingLimit = fgThreadWatchingLimit;
            }
            return this;
        }

        public Builder setBgThreadWatchingLimit(int bgThreadWatchingLimit) {
            if (bgThreadWatchingLimit > 1000) {
                config.bgThreadWatchingLimit = bgThreadWatchingLimit;
            }
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

        public Builder addLooperWatchList(String handlerThreadName) {
            if (config.looperWatchList == Collections.EMPTY_LIST) {
                config.looperWatchList = new ArrayList<>();
            }
            config.looperWatchList.add(handlerThreadName);
            return this;
        }

        public Builder addThreadWatchList(String threadName) {
            if (config.threadWatchList == Collections.EMPTY_LIST) {
                config.threadWatchList = new ArrayList<>();
            }
            config.threadWatchList.add(threadName);
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
