package com.tencent.matrix.batterycanary.monitor.plugin;


import com.tencent.matrix.batterycanary.monitor.BatteryMonitor;

public interface IBatteryMonitorPlugin {

    void onInstall(BatteryMonitor monitor);

    void onTurnOn();

    void onTurnOff();

    void onAppForeground(boolean isForeground);
}
