package com.tencent.matrix.batterycanary.monitor.plugin;


import com.tencent.matrix.batterycanary.monitor.BatteryMonitor;
import com.tencent.matrix.trace.core.LooperMonitor;

public class UITaskMonitorPlugin implements IBatteryMonitorPlugin {

    private BatteryMonitor batteryMonitor;
    private UIObserver uiObserver;
    private static final String TAG = "UITaskMonitorPlugin";

    @Override
    public void onInstall(BatteryMonitor monitor) {
        this.batteryMonitor = monitor;
        this.uiObserver = new UIObserver();
    }

    @Override
    public void onTurnOn() {
        LooperMonitor.register(uiObserver);
    }

    @Override
    public void onTurnOff() {
        LooperMonitor.unregister(uiObserver);
    }

    @Override
    public void onAppForeground(boolean isForeground) {

    }

    private void onExecute(String helpfulStr) {

    }

    class UIObserver extends LooperMonitor.LooperDispatchListener {

        @Override
        public void onDispatchStart(String x) {
            if (batteryMonitor.isAppForeground()) {
                return;
            }
            super.onDispatchStart(x);
            int begin = x.indexOf("to ");
            int end = x.lastIndexOf(":");
            onExecute(x.substring(begin + 3, end));
        }
    }


}
