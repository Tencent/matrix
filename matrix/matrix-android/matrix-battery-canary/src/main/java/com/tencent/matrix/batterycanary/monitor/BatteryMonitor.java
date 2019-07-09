package com.tencent.matrix.batterycanary.monitor;

import com.tencent.matrix.batterycanary.monitor.plugin.IBatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.JiffiesMonitorPlugin;

import java.util.LinkedList;

public class BatteryMonitor {

    private volatile boolean isTurnOn = false;
    public static volatile boolean isAppForeground = false;
    private Config config;

    public BatteryMonitor(Config config) {
        this.config = config;
        for (IBatteryMonitorPlugin plugin : config.plugins) {
            plugin.onInstall(this);
        }
    }

    public interface Printer {
        void onJiffies(JiffiesMonitorPlugin.JiffiesResult result);

        void onWakeLockTimeout(String tag, String packageName, int warningCount);
    }

    public Config getConfig() {
        return config;
    }

    public boolean isTurnOn() {
        return isTurnOn;
    }

    void turnOn() {
        if (!isTurnOn) {
            for (IBatteryMonitorPlugin plugin : config.plugins) {
                plugin.onTurnOn();
            }
            isTurnOn = true;
        }
    }

    void turnOff() {
        if (isTurnOn) {
            isTurnOn = false;
            for (IBatteryMonitorPlugin plugin : config.plugins) {
                plugin.onTurnOn();
            }
        }
    }


    public boolean isAppForeground() {
        return isAppForeground;
    }

    class Builder {
        private Config config = new Config();

        public Builder setPrinter(Printer printer) {
            config.printer = printer;
            return this;
        }

        public Builder setWakelockTimeout(long timeout) {
            config.wakelockTimeout = timeout;
            return this;
        }

        public Builder installPlugin(Class<IBatteryMonitorPlugin> pluginClass) {
            try {
                config.plugins.add(pluginClass.newInstance());
            } catch (Exception ignored) {
            }
            return this;
        }

        public Config build() {
            return config;
        }

    }

    public class Config {
        public Printer printer;
        public long wakelockTimeout;
        public LinkedList<IBatteryMonitorPlugin> plugins = new LinkedList<>();
    }


}
