package com.tencent.matrix.batterycanary.monitor.feature;


import android.os.HandlerThread;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.trace.core.LooperMonitor;

import java.util.ArrayList;
import java.util.List;

public final class LooperTaskMonitorFeature extends AbsTaskMonitorFeature {
    private static final String TAG = "Matrix.battery.LooperTaskMonitorFeature";

    public interface LooperTaskListener {
        @Deprecated
        void onTaskTrace(Thread thread, List<LooperTaskMonitorFeature.TaskTraceInfo> sortList);
        void onLooperTaskOverHeat(@NonNull List<Delta<TaskJiffiesSnapshot>> deltas);
        void onLooperConcurrentOverHeat(String key, int concurrentCount, long duringMillis);
    }

    LooperTaskListener getListener() {
        return mCore;
    }

    @Nullable
    LooperMonitor.LooperDispatchListener mLooperTaskListener;
    final List<LooperMonitor> mLooperMonitors = new ArrayList<>();

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public void onTurnOn() {
        super.onTurnOn();
        mLooperTaskListener = new LooperMonitor.LooperDispatchListener() {
            @Override
            public boolean isValid() {
                return mCore.isTurnOn();
            }

            @Override
            public void onDispatchStart(String x) {
                super.onDispatchStart(x);
                String taskName = computeTaskName(x);
                if (!TextUtils.isEmpty(taskName)) {
                    int hashcode = computeHashcode(x);
                    if (hashcode > 0) {
                        onTaskStarted(taskName, hashcode);
                    }
                }
            }

            @Override
            public void onDispatchEnd(String x) {
                super.onDispatchEnd(x);
                String taskName = computeTaskName(x);
                if (!TextUtils.isEmpty(taskName)) {
                    int hashcode = computeHashcode(x);
                    if (hashcode > 0) {
                        onTaskFinished(taskName, hashcode);
                    }
                }
            }

            private String computeTaskName(String rawInput) {
                if (TextUtils.isEmpty(rawInput)) return null;
                String symbolBgn = "} ";
                String symbolEnd = "@";
                int indexBgn = rawInput.indexOf(symbolBgn);
                int indexEnd = rawInput.lastIndexOf(symbolEnd);
                if (indexBgn >= indexEnd - 1) return null;
                return rawInput.substring(indexBgn + symbolBgn.length(), indexEnd);
            }

            private int computeHashcode(String rawInput) {
                if (TextUtils.isEmpty(rawInput)) return -1;
                String symbolBgn = "@";
                String symbolEnd = ": ";
                int indexBgn = rawInput.indexOf(symbolBgn);
                int indexEnd = rawInput.contains(symbolEnd) ? rawInput.lastIndexOf(symbolEnd) : Integer.MAX_VALUE;
                if (indexBgn >= indexEnd - 1) return -1;
                String hexString = indexEnd == Integer.MAX_VALUE ? rawInput.substring(indexBgn + symbolBgn.length()) : rawInput.substring(indexBgn + symbolBgn.length(), indexEnd);
                try {
                    return Integer.parseInt(hexString, 16);
                } catch (NumberFormatException ignored) {
                    return -1;
                }
            }
        };
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        mLooperTaskListener = null;
        for (LooperMonitor item : mLooperMonitors) {
            item.onRelease();
        }
        mLooperMonitors.clear();
    }

    @Override
    public int weight() {
        return 0;
    }

    public void watchLooper(HandlerThread handlerThread) {
        Looper looper = handlerThread.getLooper();
        watchLooper(looper);
    }

    public void watchLooper(Looper looper) {
        if (looper == null) {
            return;
        }
        if (mLooperTaskListener != null) {
            for (LooperMonitor item : mLooperMonitors) {
                if (item.getLooper() == looper) {
                    return;
                }
            }
            LooperMonitor looperMonitor = new LooperMonitor(looper);
            looperMonitor.addListener(mLooperTaskListener);
            mLooperMonitors.add(looperMonitor);
        }
    }

    @Override
    protected void onTraceOverHeat(List<Delta<TaskJiffiesSnapshot>> deltas) {
        getListener().onLooperTaskOverHeat(deltas);
    }

    @Override
    protected void onConcurrentOverHeat(String key, int concurrentCount, long duringMillis) {
        getListener().onLooperConcurrentOverHeat(key, concurrentCount, duringMillis);
    }

    @Override
    protected void onParseTaskJiffiesFail(String key, int pid, int tid) {}


    @Deprecated
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
