package com.tencent.matrix.batterycanary.monitor;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.batterycanary.monitor.plugin.IBatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.JiffiesMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.LooperTaskMonitorPlugin;
import com.tencent.matrix.plugin.Plugin;

import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.List;

public class BatteryMonitor extends Plugin {

    private volatile boolean isTurnOn = false;
    private Config config;

    public BatteryMonitor(Config config) {
        this.config = config;

        if (config.disableAppForegroundNotifyByMatrix) {
            AppActiveMatrixDelegate.INSTANCE.removeListener(this);
        }

        for (IBatteryMonitorPlugin plugin : config.plugins) {
            plugin.onInstall(this);
        }
    }

    public interface Printer {
        void onJiffies(JiffiesMonitorPlugin.JiffiesResult result);

        void onTaskTrace(Thread thread, List<LooperTaskMonitorPlugin.TaskTraceInfo> sortList);

        void onWakeLockTimeout(String tag, String packageName, int warningCount);
    }

    public Config getConfig() {
        return config;
    }

    public boolean isTurnOn() {
        return isTurnOn;
    }

    @Override
    public String getTag() {
        return "BatteryMonitor";
    }

    @Override
    public void start() {
        super.start();
        if (!isTurnOn) {
            for (IBatteryMonitorPlugin plugin : config.plugins) {
                plugin.onTurnOn();
            }
            isTurnOn = true;
        }
    }

    @Override
    public void stop() {
        super.stop();
        if (isTurnOn) {
            isTurnOn = false;
            for (IBatteryMonitorPlugin plugin : config.plugins) {
                plugin.onTurnOff();
            }
        }
    }

    private boolean isAppForeground = AppActiveMatrixDelegate.INSTANCE.isAppForeground();

    @Override
    public void onForeground(boolean isForeground) {
        isAppForeground = isForeground;
        for (IBatteryMonitorPlugin plugin : config.plugins) {
            plugin.onAppForeground(isForeground);
        }
    }

    @Override
    public boolean isForeground() {
        return isAppForeground;
    }

    public static class Builder {
        private Config config = new Config();

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

        public Config build() {
            Collections.sort(config.plugins, new Comparator<IBatteryMonitorPlugin>() {
                @Override
                public int compare(IBatteryMonitorPlugin o1, IBatteryMonitorPlugin o2) {
                    return Integer.compare(o2.weight(), o1.weight());
                }
            });
            return config;
        }

    }

    public static class Config {
        public Printer printer = new BatteryPrinter();
        public long wakelockTimeout;
        public long greyTime;
        public boolean disableAppForegroundNotifyByMatrix = false;
        public LinkedList<IBatteryMonitorPlugin> plugins = new LinkedList<>();
    }


}
