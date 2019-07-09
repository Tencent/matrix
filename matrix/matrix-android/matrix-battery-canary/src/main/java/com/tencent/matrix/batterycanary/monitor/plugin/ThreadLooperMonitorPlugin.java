package com.tencent.matrix.batterycanary.monitor.plugin;


import android.os.HandlerThread;
import android.os.Looper;
import android.util.LongSparseArray;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitor;
import com.tencent.matrix.trace.core.LooperMonitor;
import com.tencent.matrix.util.MatrixLog;

import java.util.Map;
import java.util.Set;

public class ThreadLooperMonitorPlugin implements IBatteryMonitorPlugin {

    private BatteryMonitor batteryMonitor;
    private Observer looperObserver;
    private static final String TAG = "ThreadLooperMonitorPlugin";
    private LongSparseArray<LooperMonitor> looperMonitorArray;

    @Override
    public void onInstall(BatteryMonitor monitor) {
        this.batteryMonitor = monitor;
        this.looperObserver = new Observer();
        this.looperMonitorArray = new LongSparseArray<>();
    }

    @Override
    public void onTurnOn() {

    }

    @Override
    public void onTurnOff() {
        synchronized (this) {
            for (int i = 0; i < looperMonitorArray.size(); i++) {
                LooperMonitor looperMonitor = looperMonitorArray.valueAt(i);
                looperMonitor.onRelease();
            }
            looperMonitorArray.clear();
        }
    }

    @Override
    public void onAppForeground(boolean isForeground) {
        if (batteryMonitor.isTurnOn()) {
            Map<Thread, StackTraceElement[]> stacks = Thread.getAllStackTraces();
            Set<Thread> set = stacks.keySet();
            for (Thread thread : set) {
                if (thread instanceof HandlerThread) {
                    Looper looper = ((HandlerThread) thread).getLooper();
                    if (null != looper) {
                        if (!isForeground) {
                            LooperMonitor looperMonitor = new LooperMonitor(looper);
                            looperMonitor.addListener(looperObserver);
                            synchronized (this) {
                                looperMonitorArray.put(thread.getId(), looperMonitor);
                            }
                        } else {
                            synchronized (this) {
                                LooperMonitor looperMonitor = looperMonitorArray.get(thread.getId());
                                if (null != looperMonitor) {
                                    looperMonitor.onRelease();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    protected void onExecute(Thread thread, String helpfulStr) {
        MatrixLog.d(TAG, "thread=%s helpfulStr=%s", thread.getName(), helpfulStr);
    }


    private class Observer extends LooperMonitor.LooperDispatchListener {

        @Override
        public void onDispatchStart(String x) {
            if (batteryMonitor.isAppForeground()) {
                return;
            }
            super.onDispatchStart(x);
            int begin = x.indexOf("to ");
            int end = x.lastIndexOf(":");
            onExecute(Thread.currentThread(), x.substring(begin + 3, end));
        }
    }


}
