package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Process;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.RestrictTo;
import android.support.annotation.WorkerThread;

import com.tencent.matrix.Matrix;
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
        void onParseError(int pid, int tid);
    }

    private JiffiesListener getListener() {
        return mCore;
    }

    @Override
    public int weight() {
        return Integer.MAX_VALUE;
    }

    @WorkerThread
    public JiffiesSnapshot currentJiffiesSnapshot() {
        return JiffiesSnapshot.currentJiffiesSnapshot(ProcessInfo.getProcessInfo(), mCore.getConfig().isStatPidProc, getListener());
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
                            ThreadInfo threadInfo = new ThreadInfo();
                            threadInfo.pid = pid;
                            threadInfo.tid = Integer.parseInt(file.getName());
                            threadInfoList.add(threadInfo);
                        } catch (Exception ignored) {
                        }
                    }
                    return threadInfoList;
                }
                return Collections.emptyList();
            }

            public int pid;
            public int tid;
            public String name;
            public long jiffies;

            public void loadProcStat() throws IOException {
                ProcStatUtil.ProcStat stat = ProcStatUtil.of(pid, tid);
                if (stat != null) {
                    name = stat.comm;
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
            return currentJiffiesSnapshot(processInfo, isStatPidProc, null);
        }

        public static JiffiesSnapshot currentJiffiesSnapshot(ProcessInfo processInfo, boolean isStatPidProc, @Nullable JiffiesListener listener) {
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
                    if (listener != null) {
                        listener.onParseError(processInfo.pid, 0);
                    }
                }
            }

            List<ThreadJiffiesSnapshot> threadJiffiesList = Collections.emptyList();

            if (processInfo.threadInfo.size() > 0) {
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
                        if (listener != null) {
                            listener.onParseError(threadInfo.pid, threadInfo.tid);
                        }
                    }
                }
            }
            snapshot.totalJiffies = DigitEntry.of(totalJiffies);
            snapshot.threadEntries = ListEntry.of(threadJiffiesList);
            return snapshot;
        }

        public int pid;
        public String name;
        public DigitEntry<Long> totalJiffies;
        public ListEntry<ThreadJiffiesSnapshot> threadEntries;

        private JiffiesSnapshot() {}

        @Override
        public Delta<JiffiesSnapshot> diff(JiffiesSnapshot bgn) {
            return new Delta<JiffiesSnapshot>(bgn, this) {
                @Override
                protected JiffiesSnapshot computeDelta() {
                    JiffiesSnapshot delta = new JiffiesSnapshot();
                    delta.pid = end.pid;
                    delta.name = end.name;
                    delta.totalJiffies = Differ.DigitDiffer.globalDiff(bgn.totalJiffies, end.totalJiffies);
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

            public ThreadJiffiesEntry(Long value) {
                super(value);
            }

            @Override
            public Long diff(Long right) {
                return value - right;
            }
        }
    }
}
