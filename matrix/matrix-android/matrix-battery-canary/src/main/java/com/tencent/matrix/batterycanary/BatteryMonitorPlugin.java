package com.tencent.matrix.batterycanary;

import android.app.Application;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;

public class BatteryMonitorPlugin extends Plugin {
    final BatteryMonitorCore mDelegate;

    public BatteryMonitorPlugin(BatteryMonitorCore.MonitorConfig config) {
        mDelegate = new BatteryMonitorCore(config);
    }

    @Override
    public void init(Application app, PluginListener listener) {
        super.init(app, listener);
        if (mDelegate.getConfig().disableAppForegroundNotifyByMatrix) {
            AppActiveMatrixDelegate.INSTANCE.removeListener(this);
        }
    }

    @Override
    public String getTag() {
        return "BatteryMonitorPlugin";
    }

    @Override
    public void start() {
        super.start();
        mDelegate.start();
    }

    @Override
    public void stop() {
        super.stop();
        mDelegate.stop();
    }

    @Override
    public void onForeground(boolean isForeground) {
        mDelegate.onForeground(isForeground);
    }

    @Override
    public boolean isForeground() {
        return mDelegate.isForeground();
    }
}
