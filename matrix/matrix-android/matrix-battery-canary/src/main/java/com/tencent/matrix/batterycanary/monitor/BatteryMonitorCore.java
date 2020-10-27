package com.tencent.matrix.batterycanary.monitor;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.batterycanary.monitor.plugin.IBatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.JiffiesMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.LooperTaskMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.WakeLockMonitorPlugin;

import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.List;

public class BatteryMonitorCore implements JiffiesMonitorPlugin.JiffiesListener, LooperTaskMonitorPlugin.LooperTaskListener, WakeLockMonitorPlugin.WakeLockListener {
    private volatile boolean isTurnOn = false;
    private boolean isAppForeground = AppActiveMatrixDelegate.INSTANCE.isAppForeground();
    private final MonitorConfig config;

    public BatteryMonitorCore(MonitorConfig config) {
        this.config = config;
        if (config.printer instanceof BatteryPrinter) ((BatteryPrinter) config.printer).attach(this);
        for (IBatteryMonitorPlugin plugin : config.plugins) {
            plugin.onInstall(this);
        }
    }

    public <T extends IBatteryMonitorPlugin> T getMonitorPlugin(Class<T> clazz) {
        for (IBatteryMonitorPlugin plugin : config.plugins) {
            if (clazz.isAssignableFrom(plugin.getClass())) {
                //noinspection unchecked
                return (T) plugin;
            }
        }
        return null;
    }

    public MonitorConfig getConfig() {
        return config;
    }

    public boolean isTurnOn() {
        synchronized (BatteryMonitorCore.class) {
            return isTurnOn;
        }
    }

    public void start() {
        synchronized (BatteryMonitorCore.class) {
            if (!isTurnOn) {
                for (IBatteryMonitorPlugin plugin : config.plugins) {
                    plugin.onTurnOn();
                }
                isTurnOn = true;
            }
        }
    }

    public void stop() {
        synchronized (BatteryMonitorCore.class) {
            if (isTurnOn) {
                for (IBatteryMonitorPlugin plugin : config.plugins) {
                    plugin.onTurnOff();
                }
                isTurnOn = false;
            }
        }
    }

    public void onForeground(boolean isForeground) {
        isAppForeground = isForeground;
        for (IBatteryMonitorPlugin plugin : config.plugins) {
            plugin.onAppForeground(isForeground);
        }
    }

    public boolean isForeground() {
        return isAppForeground;
    }

    @Override
    public void onTraceBegin() {
        getConfig().printer.onTraceBegin();
    }

    @Override
    public void onTraceEnd() {
        getConfig().printer.onTraceEnd();
    }

    @Override
    public void onJiffies(JiffiesMonitorPlugin.JiffiesResult result) {
        getConfig().printer.onJiffies(result);
    }

    @Override
    public void onTaskTrace(Thread thread, List<LooperTaskMonitorPlugin.TaskTraceInfo> sortList) {
        getConfig().printer.onTaskTrace(thread, sortList);
    }

    @Override
    public void onWakeLockTimeout(String tag, String packageName, int warningCount) {
        onWakeLockTimeout(tag, packageName, warningCount);
    }

    public interface Printer extends JiffiesMonitorPlugin.JiffiesListener, LooperTaskMonitorPlugin.LooperTaskListener, WakeLockMonitorPlugin.WakeLockListener {}

    public static class MonitorConfig {
        public Printer printer = new BatteryPrinter();
        public long wakelockTimeout;
        public long greyTime;
        public long foregroundLoopCheckTime;
        public boolean isEnableCheckForeground = false;
        public boolean disableAppForegroundNotifyByMatrix = false;
        public LinkedList<IBatteryMonitorPlugin> plugins = new LinkedList<>();

        public static class Builder {
            private MonitorConfig config = new MonitorConfig();

            public Builder setPrinter(Printer printer) {
                config.printer = printer;
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

            public Builder installPlugin(Class<? extends IBatteryMonitorPlugin> pluginClass) {
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

            public MonitorConfig build() {
                Collections.sort(config.plugins, new Comparator<IBatteryMonitorPlugin>() {
                    @Override
                    public int compare(IBatteryMonitorPlugin o1, IBatteryMonitorPlugin o2) {
                        return Integer.compare(o2.weight(), o1.weight());
                    }
                });
                return config;
            }
        }
    }
}
