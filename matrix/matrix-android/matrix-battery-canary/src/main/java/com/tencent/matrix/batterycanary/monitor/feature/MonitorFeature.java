package com.tencent.matrix.batterycanary.monitor.feature;


import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;

public interface MonitorFeature {
    void configure(BatteryMonitorCore monitor);
    void onTurnOn();
    void onTurnOff();
    void onForeground(boolean isForeground);
    int weight();
}
