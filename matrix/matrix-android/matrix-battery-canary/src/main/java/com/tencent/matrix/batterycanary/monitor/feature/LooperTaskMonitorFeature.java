package com.tencent.matrix.batterycanary.monitor.feature;


import android.os.HandlerThread;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.util.LongSparseArray;

import com.tencent.matrix.trace.core.LooperMonitor;

import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

@SuppressWarnings("NotNullFieldNotInitialized")
public final class LooperTaskMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.LooperTaskMonitorFeature";

    public interface LooperTaskListener {
        void onTaskTrace(Thread thread, List<LooperTaskMonitorFeature.TaskTraceInfo> sortList);
    }
    private final LongSparseArray<LooperMonitor> looperMonitorArray = new LongSparseArray<>();
    private static final int MAX_CHAT_COUNT = 60;

    private LooperTaskListener getListener() {
        return mCore;
    }

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        release();
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        if (mCore.isTurnOn()) {
            Map<Thread, StackTraceElement[]> stacks = Thread.getAllStackTraces();
            Set<Thread> set = stacks.keySet();

            // Iterate HandlerThread
            for (Thread thread : set) {
                if (thread instanceof HandlerThread) {
                    Looper looper = ((HandlerThread) thread).getLooper();
                    if (null != looper) {

                        // Looper Tracing:
                        // Start tracing when bg, finish tracing when fg
                        if (isForeground) {
                            List<TaskTraceInfo> list = onLooperTraceFinish((HandlerThread) thread);
                            if (!list.isEmpty()) {
                                getListener().onTaskTrace(thread, list);
                            }
                        } else {
                            onLooperTraceStart((HandlerThread) thread);
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

    private void release() {
        synchronized (looperMonitorArray) {
            for (int i = 0; i < looperMonitorArray.size(); i++) {
                looperMonitorArray.valueAt(i).onRelease();
            }
            looperMonitorArray.clear();
        }
    }

    @WorkerThread
    public void onLooperTraceStart(HandlerThread thread) {
        Looper looper = thread.getLooper();
        if (looper == null) {
            return;
        }
        synchronized (looperMonitorArray) {
            if (looperMonitorArray.get(thread.getId()) == null) {
                LooperMonitor looperMonitor = new LooperMonitor(looper);
                looperMonitor.addListener(new Observer());
                looperMonitorArray.put(thread.getId(), looperMonitor);
            }
        }
    }

    @WorkerThread
    public List<TaskTraceInfo> onLooperTraceFinish(HandlerThread thread) {
        LooperMonitor looperMonitor;
        synchronized (looperMonitorArray) {
            looperMonitor = looperMonitorArray.get(thread.getId());
            if (null != looperMonitor) {
                looperMonitorArray.remove(thread.getId());
            }
        }

        List<TaskTraceInfo> list = Collections.emptyList();
        if (null != looperMonitor) {
            for (LooperMonitor.LooperDispatchListener listener : looperMonitor.getListeners()) {
                if (listener instanceof Observer) {
                    Map<String, TaskTraceInfo> taskMap = ((Observer) listener).map;
                    if (!taskMap.isEmpty()) {
                        if (list.isEmpty()) {
                            list = new LinkedList<>();
                        }
                        list.addAll(taskMap.values());
                        taskMap.clear();
                    }
                    break;
                }
            }
            looperMonitor.onRelease();
        }

        if (!list.isEmpty()) {
            Collections.sort(list, new Comparator<TaskTraceInfo>() {
                @Override
                public int compare(TaskTraceInfo o1, TaskTraceInfo o2) {
                    return Integer.compare(o2.count, o1.count);
                }
            });
        }
        return list;
    }


    private class Observer extends LooperMonitor.LooperDispatchListener {
        private final HashMap<String, TaskTraceInfo> map = new HashMap<>();

        public Observer() {}

        @Override
        public boolean isValid() {
            return !mCore.isForeground();
        }

        @Override
        public void onDispatchStart(String x) {
            if (mCore.isForeground()) {
                return;
            }
            super.onDispatchStart(x);
            int begin = x.indexOf("to ") + 3;
            int last = x.lastIndexOf('@');
            if (last < 0) {
                last = x.lastIndexOf(':');
            }
            int end = Math.max(last - MAX_CHAT_COUNT, begin);
            onTaskTrace(x.substring(end));
        }

        protected void onTaskTrace(String helpfulStr) {
            TaskTraceInfo traceInfo = map.get(helpfulStr);
            if (traceInfo == null) {
                traceInfo = new TaskTraceInfo();
                map.put(helpfulStr, traceInfo);
            }
            traceInfo.helpfulStr = helpfulStr;
            traceInfo.increment();
        }
    }


    public static class TaskTraceInfo {
        private static final int LENGTH = 1000;
        private int count;
        private long[] times;
        String helpfulStr;

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
            if (helpfulStr == null) return false;
            if (!(obj instanceof String)) return false;
            return helpfulStr.equals(obj);
        }
    }
}
