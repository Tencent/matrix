package com.tencent.matrix.batterycanary.monitor.plugin;


import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

public interface IBatteryMonitorPlugin {

    void onInstall(BatteryMonitorCore monitor);

    void onTurnOn();

    void onTurnOff();

    void onAppForeground(boolean isForeground);

    int weight();

}
