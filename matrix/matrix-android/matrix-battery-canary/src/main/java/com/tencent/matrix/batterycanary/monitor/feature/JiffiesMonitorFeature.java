package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Process;
import android.os.SystemClock;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;
import com.tencent.matrix.batterycanary.utils.BatteryCanaryUtil;
import com.tencent.matrix.batterycanary.utils.ProcStatUtil;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

@SuppressWarnings("NotNullFieldNotInitialized")
public final class JiffiesMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.JiffiesMonitorFeature";

    public interface JiffiesListener {
        @Deprecated
        void onParseError(int pid, int tid);

        void onWatchingThreads(ListEntry<? extends JiffiesSnapshot.ThreadJiffiesEntry> threadJiffiesList);
    }

    private final ThreadWatchDog mFgThreadWatchDog = new ThreadWatchDog();
    private final ThreadWatchDog mBgThreadWatchDog = new ThreadWatchDog();

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    public int weight() {
        return Integer.MAX_VALUE;
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        if (isForeground) {
            mFgThreadWatchDog.start();
            mBgThreadWatchDog.stop();
        } else {
            mBgThreadWatchDog.start();
            mFgThreadWatchDog.stop();
        }
    }

    public void watchBackThreadSate(boolean isForeground, int pid, int tid) {
        if (isForeground) {
            mFgThreadWatchDog.watch(pid, tid);
        } else {
            mBgThreadWatchDog.watch(pid, tid);
        }
    }

    @WorkerThread
    public JiffiesSnapshot currentJiffiesSnapshot() {
        return JiffiesSnapshot.currentJiffiesSnapshot(ProcessInfo.getProcessInfo(), mCore.getConfig().isStatPidProc);
    }

    @AnyThread
    public void currentJiffiesSnapshot(@NonNull final BatteryMonitorCore.Callback<JiffiesSnapshot> callback) {
        mCore.getHandler().post(new Runnable() {
            @Override
            public void run() {
                callback.onGetJiffies(currentJiffiesSnapshot());
            }
        });
    }

    @SuppressWarnings("SpellCheckingInspection")
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public static class ProcessInfo {
        static ProcessInfo getProcessInfo() {
            ProcessInfo processInfo = new ProcessInfo();
            processInfo.pid = Process.myPid();
            processInfo.name = Matrix.isInstalled() ? MatrixUtil.getProcessName(Matrix.with().getApplication()) : "default";
            processInfo.threadInfo = ThreadInfo.parseThreadsInfo(processInfo.pid);
            processInfo.upTime = SystemClock.uptimeMillis();
            processInfo.time = System.currentTimeMillis();
            return processInfo;
        }

        int pid;
        String name;
        long time;
        long upTime;
        public long jiffies;
        List<ThreadInfo> threadInfo = Collections.emptyList();

        public void loadProcStat() throws IOException {
            ProcStatUtil.ProcStat stat = ProcStatUtil.of(pid);
            if (stat != null) {
                name = stat.comm;
                jiffies = stat.getJiffies();
            } else {
                throw new IOException("parse fail: " + BatteryCanaryUtil.cat("/proc/" + pid + "/stat"));
            }
        }

        @NonNull
        @Override
        public String toString() {
            return "process:" + name + "(" + pid + ") " + "thread size:" + threadInfo.size();
        }

        @SuppressWarnings("SpellCheckingInspection")
        public static class ThreadInfo {
            private static List<ThreadInfo> parseThreadsInfo(int pid) {
                String rootPath = "/proc/" + pid + "/task/";
                File taskDir = new File(rootPath);
                try {
                    if (taskDir.isDirectory()) {
                        File[] subDirs = taskDir.listFiles();
                        if (null == subDirs) {
                            return Collections.emptyList();
                        }

                        List<ThreadInfo> threadInfoList = new ArrayList<>(subDirs.length);
                        for (File file : subDirs) {
                            if (!file.isDirectory()) {
                                continue;
                            }
                            try {
                                ThreadInfo threadInfo = of(pid, Integer.parseInt(file.getName()));
                                threadInfoList.add(threadInfo);
                            } catch (Exception e) {
                                MatrixLog.printErrStackTrace(TAG, e, "parse thread error: " + file.getName());
                            }
                        }
                        return threadInfoList;
                    }
                } catch (Exception e) {
                    MatrixLog.printErrStackTrace(TAG, e, "list thread dir error");
                }
                return Collections.emptyList();
            }

            private static ThreadInfo of(int pid, int tid) {
                ThreadInfo threadInfo = new ThreadInfo();
                threadInfo.pid = pid;
                threadInfo.tid = tid;
                return threadInfo;
            }

            public int pid;
            public int tid;
            public String name;
            public String stat;
            public long jiffies;

            public void loadProcStat() throws IOException {
                ProcStatUtil.ProcStat stat = ProcStatUtil.of(pid, tid);
                if (stat != null && !TextUtils.isEmpty(stat.comm)) {
                    this.name = stat.comm;
                    this.stat = stat.stat;
                    jiffies = stat.getJiffies();
                } else {
                    throw new IOException("parse fail: " + BatteryCanaryUtil.cat("/proc/" + pid + "/task/" + tid + "/stat"));
                }
            }

            @NonNull
            @Override
            public String toString() {
                return "thread:" + name + "(" + tid + ") " + jiffies;
            }
        }
    }

    @SuppressWarnings({"SpellCheckingInspection", "deprecation"})
    public static class JiffiesSnapshot extends Snapshot<JiffiesSnapshot> {
        public static JiffiesSnapshot currentJiffiesSnapshot(ProcessInfo processInfo, boolean isStatPidProc) {
            JiffiesSnapshot snapshot = new JiffiesSnapshot();
            snapshot.pid = processInfo.pid;
            snapshot.name = processInfo.name;

            long totalJiffies = 0;
            if (isStatPidProc) {
                try {
                    // idividually configure pids' jiffies
                    processInfo.loadProcStat();
                    totalJiffies = processInfo.jiffies;
                } catch (IOException e) {
                    MatrixLog.printErrStackTrace(TAG, e, "parseProcJiffies fail");
                    isStatPidProc = false;
                    snapshot.setValid(false);
                }
            }

            List<ThreadJiffiesSnapshot> threadJiffiesList = Collections.emptyList();
            int threadNum = 0;

            if (processInfo.threadInfo.size() > 0) {
                threadNum = processInfo.threadInfo.size();
                threadJiffiesList = new ArrayList<>(processInfo.threadInfo.size());
                for (ProcessInfo.ThreadInfo threadInfo : processInfo.threadInfo) {
                    ThreadJiffiesSnapshot threadJiffies = ThreadJiffiesSnapshot.parseThreadJiffies(threadInfo);
                    if (threadJiffies != null) {
                        threadJiffiesList.add(threadJiffies);
                        if (!isStatPidProc) {
                            // acc of all tids' jiffies
                            totalJiffies += threadJiffies.value;
                        }
                    } else {
                        snapshot.setValid(false);
                    }
                }
            }
            snapshot.totalJiffies = DigitEntry.of(totalJiffies);
            snapshot.threadEntries = ListEntry.of(threadJiffiesList);
            snapshot.threadNum = DigitEntry.of(threadNum);
            return snapshot;
        }

        public int pid;
        public String name;
        public DigitEntry<Long> totalJiffies;
        public ListEntry<ThreadJiffiesSnapshot> threadEntries;
        public DigitEntry<Integer> threadNum;

        private JiffiesSnapshot() {
        }

        @Override
        public Delta<JiffiesSnapshot> diff(JiffiesSnapshot bgn) {
            return new Delta<JiffiesSnapshot>(bgn, this) {
                @Override
                protected JiffiesSnapshot computeDelta() {
                    JiffiesSnapshot delta = new JiffiesSnapshot();
                    delta.pid = end.pid;
                    delta.name = end.name;
                    delta.totalJiffies = Differ.DigitDiffer.globalDiff(bgn.totalJiffies, end.totalJiffies);
                    delta.threadNum = Differ.DigitDiffer.globalDiff(bgn.threadNum, end.threadNum);
                    delta.threadEntries = ListEntry.ofEmpty();

                    if (end.threadEntries.getList().size() > 0) {
                        List<ThreadJiffiesSnapshot> deltaThreadEntries = new ArrayList<>();
                        for (ThreadJiffiesSnapshot endRecord : end.threadEntries.getList()) {
                            boolean isNewAdded = true;
                            long jiffiesConsumed = endRecord.value;
                            for (ThreadJiffiesSnapshot bgnRecord : bgn.threadEntries.getList()) {
                                if (bgnRecord.name.equals(endRecord.name) && bgnRecord.tid == endRecord.tid) {
                                    isNewAdded = false;
                                    jiffiesConsumed = Differ.DigitDiffer.globalDiff(bgnRecord, endRecord).value;
                                    break;
                                }
                            }
                            if (jiffiesConsumed > 0) {
                                ThreadJiffiesSnapshot deltaThreadJiffies = new ThreadJiffiesSnapshot(jiffiesConsumed);
                                deltaThreadJiffies.tid = endRecord.tid;
                                deltaThreadJiffies.name = endRecord.name;
                                deltaThreadJiffies.stat = endRecord.stat;
                                deltaThreadJiffies.isNewAdded = isNewAdded;
                                deltaThreadEntries.add(deltaThreadJiffies);
                            }
                        }
                        if (deltaThreadEntries.size() > 0) {
                            Collections.sort(deltaThreadEntries, new Comparator<ThreadJiffiesSnapshot>() {
                                @Override
                                public int compare(ThreadJiffiesSnapshot o1, ThreadJiffiesSnapshot o2) {
                                    long minus = o1.get() - o2.get();
                                    if (minus == 0) return 0;
                                    if (minus > 0) return -1;
                                    return 1;
                                }
                            });
                            delta.threadEntries = ListEntry.of(deltaThreadEntries);
                        }
                    }
                    return delta;
                }
            };
        }

        /**
         * Use {@link ThreadJiffiesEntry} instead.
         */
        @Deprecated
        public static class ThreadJiffiesSnapshot extends ThreadJiffiesEntry {
            @Nullable
            public static ThreadJiffiesSnapshot parseThreadJiffies(ProcessInfo.ThreadInfo threadInfo) {
                try {
                    threadInfo.loadProcStat();
                    ThreadJiffiesSnapshot snapshot = new ThreadJiffiesSnapshot(threadInfo.jiffies);
                    snapshot.name = threadInfo.name;
                    snapshot.stat = threadInfo.stat;
                    snapshot.tid = threadInfo.tid;
                    snapshot.isNewAdded = true;
                    return snapshot;
                } catch (IOException e) {
                    MatrixLog.printErrStackTrace(TAG, e, "parseThreadJiffies fail");
                    return null;
                }
            }

            public ThreadJiffiesSnapshot(Long value) {
                super(value);
            }
        }

        public static class ThreadJiffiesEntry extends DigitEntry<Long> {
            public int tid;
            @NonNull
            public String name;
            public boolean isNewAdded;
            @NonNull
            public String stat;

            public ThreadJiffiesEntry(Long value) {
                super(value);
            }

            @Override
            public Long diff(Long right) {
                return value - right;
            }
        }
    }

    class ThreadWatchDog implements Runnable {
        private long duringMillis;
        private final List<ProcessInfo.ThreadInfo> mWatchingThreads = new ArrayList<>();

        @Override
        public void run() {
            // watch
            MatrixLog.i(TAG, "threadWatchDog start, size = " + mWatchingThreads.size()
                    + ", delayMillis = " + duringMillis);

            List<JiffiesSnapshot.ThreadJiffiesSnapshot> threadJiffiesList = new ArrayList<>();
            synchronized (mWatchingThreads) {
                for (ProcessInfo.ThreadInfo item : mWatchingThreads) {
                    JiffiesSnapshot.ThreadJiffiesSnapshot snapshot = JiffiesSnapshot.ThreadJiffiesSnapshot.parseThreadJiffies(item);
                    if (snapshot != null) {
                        snapshot.isNewAdded = false;
                        threadJiffiesList.add(snapshot);
                    }
                }
            }
            if (!threadJiffiesList.isEmpty()) {
                ListEntry<JiffiesSnapshot.ThreadJiffiesSnapshot> threadJiffiesListEntry = ListEntry.of(threadJiffiesList);
                mCore.getConfig().callback.onWatchingThreads(threadJiffiesListEntry);
            }

            // next loop
            if (duringMillis <= 5 * 60 * 1000L) {
                mCore.getHandler().postDelayed(this, setNext(5 * 60 * 1000L));
            } else if (duringMillis <= 10 * 60 * 1000L) {
                mCore.getHandler().postDelayed(this, setNext(10 * 60 * 1000L));
            } else {
                // done
                synchronized (mWatchingThreads) {
                    mWatchingThreads.clear();
                }
            }
        }

        void watch(int pid, int tid) {
            synchronized (mWatchingThreads) {
                // Distinct
                for (ProcessInfo.ThreadInfo item : mWatchingThreads) {
                    if (item.pid == pid && item.tid == tid) {
                        return;
                    }
                }
                mWatchingThreads.add(ProcessInfo.ThreadInfo.of(pid, tid));
            }
        }

        void start() {
            synchronized (mWatchingThreads) {
                MatrixLog.i(TAG, "ThreadWatchDog start watching, count = " + mWatchingThreads.size());
                if (!mWatchingThreads.isEmpty()) {
                    mCore.getHandler().postDelayed(this, reset());
                }
            }
        }

        void stop() {
            mCore.getHandler().removeCallbacks(this);
        }

        private long reset() {
            duringMillis = 0L;
            setNext(5 * 60 * 1000L);
            return duringMillis;
        }

        private long setNext(long millis) {
            duringMillis += millis;
            return millis;
        }
    }
}
