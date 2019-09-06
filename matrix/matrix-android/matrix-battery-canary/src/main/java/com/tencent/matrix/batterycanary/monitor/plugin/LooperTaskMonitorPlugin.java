package com.tencent.matrix.batterycanary.monitor.plugin;


import android.os.HandlerThread;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.LongSparseArray;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitor;
import com.tencent.matrix.trace.core.LooperMonitor;
import com.tencent.matrix.util.MatrixLog;

import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class LooperTaskMonitorPlugin implements IBatteryMonitorPlugin {

    private BatteryMonitor batteryMonitor;
    private static final String TAG = "Matrix.LooperTaskMonitorPlugin";
    private LongSparseArray<LooperMonitor> looperMonitorArray;
    private LongSparseArray<ITaskTracer> taskTracers;

    @Override
    public void onInstall(BatteryMonitor monitor) {
        MatrixLog.i(TAG, "onInstall");
        this.batteryMonitor = monitor;
        this.looperMonitorArray = new LongSparseArray<>();
        this.taskTracers = new LongSparseArray<>();
    }

    @Override
    public void onTurnOn() {
        MatrixLog.i(TAG, "onTurnOn");
    }

    @Override
    public void onTurnOff() {
        MatrixLog.i(TAG, "onTurnOff");
        onUnbindLooperMonitor();
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
                            onBindLooperMonitor(thread, looper);
                        } else {
                            List<TaskTraceInfo> list = onUnbindLooperMonitor(thread);
                            if (batteryMonitor.getConfig().printer != null && !list.isEmpty()) {
                                batteryMonitor.getConfig().printer.onTaskTrace(thread, list);
                            }
                        }
                    }
                }
            }
        }
    }

    @Override
    public int weight() {
        return 0;
    }

    public void addTaskTracer(Thread thread, ITaskTracer taskTracer) {
        synchronized (taskTracers) {
            taskTracers.put(thread.getId(), taskTracer);
        }
    }

    public void removeTaskTracer(Thread thread) {
        synchronized (taskTracers) {
            taskTracers.remove(thread.getId());
        }
    }

    public void onBindLooperMonitor(Thread thread, Looper looper) {
        LooperMonitor looperMonitor = new LooperMonitor(looper);
        synchronized (taskTracers) {
            looperMonitor.addListener(new Observer(taskTracers.get(thread.getId())));
        }
        synchronized (looperMonitorArray) {
            looperMonitorArray.put(thread.getId(), looperMonitor);
        }
    }

    private void onUnbindLooperMonitor() {
        synchronized (looperMonitorArray) {
            for (int i = 0; i < looperMonitorArray.size(); i++) {
                LooperMonitor looperMonitor = looperMonitorArray.valueAt(i);
                looperMonitor.onRelease();
            }
            looperMonitorArray.clear();
        }
    }

    public LinkedList<TaskTraceInfo> onUnbindLooperMonitor(Thread thread) {
        LooperMonitor looperMonitor;
        synchronized (looperMonitorArray) {
            looperMonitor = looperMonitorArray.get(thread.getId());
            if(null != looperMonitor){
                looperMonitorArray.remove(thread.getId());
            }
        }
        LinkedList<TaskTraceInfo> list = new LinkedList<>();
        if (null != looperMonitor) {
            for (LooperMonitor.LooperDispatchListener listener : looperMonitor.getListeners()) {
                if (listener instanceof Observer) {
                    list.addAll(((Observer) listener).map.values());
                    Collections.sort(list, new Comparator<TaskTraceInfo>() {
                        @Override
                        public int compare(TaskTraceInfo o1, TaskTraceInfo o2) {
                            return Integer.compare(o2.count, o1.count);
                        }
                    });
                    break;
                }
            }
            looperMonitor.onRelease();
        }
        return list;
    }


    private class Observer extends LooperMonitor.LooperDispatchListener {

        private HashMap<String, TaskTraceInfo> map = new HashMap<>();

        private ITaskTracer taskTracer;

        public Observer(ITaskTracer taskTracer) {
            this.taskTracer = taskTracer;
        }

        @Override
        public boolean isValid() {
            return !batteryMonitor.isForeground();
        }

        @Override
        public void onDispatchStart(String x) {
            if (batteryMonitor.isForeground()) {
                return;
            }
            super.onDispatchStart(x);
            int begin = x.indexOf("to ");
            onTaskTrace(Thread.currentThread(), x.substring(begin + 3));
        }

        protected void onTaskTrace(Thread thread, String helpfulStr) {
            TaskTraceInfo traceInfo = map.get(helpfulStr);
            if (traceInfo == null) {
                traceInfo = new TaskTraceInfo();
                map.put(helpfulStr, traceInfo);
            }
            traceInfo.helpfulStr = helpfulStr;
            traceInfo.increment();

            if (null != taskTracer) {
                taskTracer.onTaskTrace(thread, helpfulStr);
            }
        }
    }

    public interface ITaskTracer {
        void onTaskTrace(Thread thread, String helpfulStr);
    }


    public class TaskTraceInfo {
        private static final int LENGTH = 1000;
        private int count;
        String helpfulStr;
        private long[] times;

        void increment() {
            if (times == null) {
                times = new long[LENGTH];
            }
            int index = count % LENGTH;
            times[index] = System.currentTimeMillis();
            count++;
        }

        @NonNull
        @Override
        public String toString() {
            return helpfulStr + "=" + count;
        }

        @Override
        public int hashCode() {
            return helpfulStr.hashCode();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            return helpfulStr.equals(obj);
        }
    }


}
