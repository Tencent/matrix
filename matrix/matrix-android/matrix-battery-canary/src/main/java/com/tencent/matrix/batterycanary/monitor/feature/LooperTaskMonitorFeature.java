package com.tencent.matrix.batterycanary.monitor.feature;


import android.os.HandlerThread;
import android.os.Looper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import android.text.TextUtils;

import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Delta;
import com.tencent.matrix.trace.core.LooperMonitor;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class LooperTaskMonitorFeature extends AbsTaskMonitorFeature {
    private static final String TAG = "Matrix.battery.LooperTaskMonitorFeature";

    public interface LooperTaskListener {
        @Deprecated
        void onTaskTrace(Thread thread, List<LooperTaskMonitorFeature.TaskTraceInfo> sortList);

        void onLooperTaskOverHeat(@NonNull List<Delta<TaskJiffiesSnapshot>> deltas);

        void onLooperConcurrentOverHeat(String key, int concurrentCount, long duringMillis);
    }

    final List<String> mWatchingList = new ArrayList<>();
    final Map<Looper, LooperMonitor> mLooperMonitorTrace = new HashMap<>();

    @Nullable
    LooperMonitor.LooperDispatchListener mLooperTaskListener;
    @Nullable
    Runnable mDelayWatchingTask;

    @Override
    protected String getTag() {
        return TAG;
    }

    LooperTaskListener getListener() {
        return mCore;
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
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        if (isForeground) {
            if (mDelayWatchingTask != null) {
                mCore.getHandler().removeCallbacks(mDelayWatchingTask);
            }
        } else {
            mDelayWatchingTask = new Runnable() {
                @Override
                public void run() {
                    startWatching();
                }
            };
            mCore.getHandler().postDelayed(mDelayWatchingTask, mCore.getConfig().greyTime);
        }
    }

    @Override
    public void onTurnOff() {
        super.onTurnOff();
        stopWatching();
    }

    @Override
    public int weight() {
        return 0;
    }

    void startWatching() {
        synchronized (mWatchingList) {
            if (mLooperTaskListener == null) {
                return;
            }
            MatrixLog.i(TAG, "#startWatching");
            if (mCore.getConfig().looperWatchList.contains("all")) {
                // 1. Update watching for all handler threads
                Collection<Thread> allThreads = getAllThreads();
                for (Thread thread : allThreads) {
                    if (thread instanceof HandlerThread) {
                        Looper looper = ((HandlerThread) thread).getLooper();
                        if (looper != null) {
                            if (!mLooperMonitorTrace.containsKey(looper)) {
                                // update watch
                                watchLooper((HandlerThread) thread);
                            }
                        }
                    }
                }

            } else {
                // 2. Update watching for configured threads
                Collection<Thread> allThreads = Collections.emptyList();
                for (String threadToWatch : mCore.getConfig().looperWatchList) {
                    if (TextUtils.isEmpty(threadToWatch)) {
                        continue;
                    }
                    if (!mWatchingList.contains(threadToWatch)) {
                        if (allThreads.isEmpty()) {
                            // for lazy load
                            allThreads = getAllThreads();
                        }
                        for (Thread thread : allThreads) {
                            if (thread.getName().contains(threadToWatch)) {
                                if (thread instanceof HandlerThread) {
                                    Looper looper = ((HandlerThread) thread).getLooper();
                                    if (looper != null) {
                                        // update watch
                                        watchLooper(threadToWatch, looper);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private Collection<Thread> getAllThreads() {
        Map<Thread, StackTraceElement[]> stackTraces = Thread.getAllStackTraces();
        return stackTraces == null ? Collections.<Thread>emptyList() : stackTraces.keySet();
    }

    void stopWatching() {
        synchronized (mWatchingList) {
            mLooperTaskListener = null;
            for (LooperMonitor item : mLooperMonitorTrace.values()) {
                item.onRelease();
            }
            mLooperMonitorTrace.clear();
            mWatchingList.clear();
        }
    }

    public void watchLooper(HandlerThread handlerThread) {
        Looper looper = handlerThread.getLooper();
        watchLooper(handlerThread.getName(), looper);
    }

    void watchLooper(String name, Looper looper) {
        if (TextUtils.isEmpty(name) || looper == null) {
            return;
        }

        synchronized (mWatchingList) {
            if (mLooperTaskListener != null) {
                // remove if existing
                mWatchingList.remove(name);
                LooperMonitor remove = mLooperMonitorTrace.remove(looper);
                if (remove != null) {
                    remove.onRelease();
                }
                // add looper tracing
                LooperMonitor looperMonitor = new LooperMonitor(looper);
                looperMonitor.addListener(mLooperTaskListener);
                mWatchingList.add(name);
                mLooperMonitorTrace.put(looper, looperMonitor);
            }
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
    protected void onParseTaskJiffiesFail(String key, int pid, int tid) {
    }


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
