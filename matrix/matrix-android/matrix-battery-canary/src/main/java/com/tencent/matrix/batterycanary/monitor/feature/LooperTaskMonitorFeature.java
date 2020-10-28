package com.tencent.matrix.batterycanary.monitor.feature;


import android.os.HandlerThread;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.LongSparseArray;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.trace.core.LooperMonitor;
import com.tencent.matrix.util.MatrixLog;

import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class LooperTaskMonitorFeature implements MonitorFeature {
    private static final String TAG = "Matrix.LooperTaskMonitorPlugin";

    public interface LooperTaskListener {
        void onTaskTrace(Thread thread, List<LooperTaskMonitorFeature.TaskTraceInfo> sortList);
    }

    private BatteryMonitorCore monitor;
    private final LongSparseArray<LooperMonitor> looperMonitorArray = new LongSparseArray<>();
    private static final int MAX_CHAT_COUNT = 60;

    private LooperTaskListener getListener() {
        return monitor;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "onInstall");
        this.monitor = monitor;
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
        if (monitor.isTurnOn()) {
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
                            if (getListener() != null && !list.isEmpty()) {
                                getListener().onTaskTrace(thread, list);
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


    public void onBindLooperMonitor(Thread thread, Looper looper) {
        synchronized (looperMonitorArray) {
            if (looperMonitorArray.get(thread.getId()) == null) {
                looperMonitorArray.put(thread.getId(), new LooperMonitor(looper));
            }
        }
    }

    private void onUnbindLooperMonitor() {
        synchronized (looperMonitorArray) {
            for (int i = 0; i < looperMonitorArray.size(); i++) {
                looperMonitorArray.valueAt(i).onRelease();
            }
            looperMonitorArray.clear();
        }
    }

    public LinkedList<TaskTraceInfo> onUnbindLooperMonitor(Thread thread) {
        LooperMonitor looperMonitor;
        synchronized (looperMonitorArray) {
            looperMonitor = looperMonitorArray.get(thread.getId());
            if (null != looperMonitor) {
                looperMonitorArray.remove(thread.getId());
            }
        }
        LinkedList<TaskTraceInfo> list = new LinkedList<>();
        if (null != looperMonitor) {
            for (LooperMonitor.LooperDispatchListener listener : looperMonitor.getListeners()) {
                if (listener instanceof Observer) {
                    Map<String, TaskTraceInfo> taskMap = ((Observer) listener).map;
                    list.addAll(taskMap.values());
                    taskMap.clear();
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
            return !monitor.isForeground();
        }

        @Override
        public void onDispatchStart(String x) {
            if (monitor.isForeground()) {
                return;
            }
            super.onDispatchStart(x);
            int begin = x.indexOf("to ") + 3;
            int last = x.lastIndexOf('@');
            if (last < 0) {
                last = x.lastIndexOf(':');
            }
            int end = Math.max(last - MAX_CHAT_COUNT, begin);
            onTaskTrace(Thread.currentThread(), x.substring(end));
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


    public static class TaskTraceInfo {
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
