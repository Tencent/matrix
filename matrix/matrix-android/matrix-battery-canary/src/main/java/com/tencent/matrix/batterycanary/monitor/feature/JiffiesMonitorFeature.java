package com.tencent.matrix.batterycanary.monitor.feature;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorConfig;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore.Callback;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.DigitEntry;
import com.tencent.matrix.batterycanary.monitor.feature.MonitorFeature.Snapshot.Entry.ListEntry;
import com.tencent.matrix.batterycanary.shell.TopThreadFeature;
import com.tencent.matrix.batterycanary.shell.ui.TopThreadIndicator;
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

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.annotation.WorkerThread;
import androidx.core.util.Pair;

@SuppressWarnings("NotNullFieldNotInitialized")
public final class JiffiesMonitorFeature extends AbsMonitorFeature {
    private static final String TAG = "Matrix.battery.JiffiesMonitorFeature";
    private static boolean sSkipNewAdded = false; // FIXME: move to feature's configuration

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
    public void onTurnOn() {
        super.onTurnOn();
        sSkipNewAdded = mCore.getConfig().isSkipNewAddedPidTid;
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

    @WorkerThread
    public JiffiesSnapshot currentJiffiesSnapshot(int pid) {
        return JiffiesSnapshot.currentJiffiesSnapshot(ProcessInfo.getProcessInfo(pid), mCore.getConfig().isStatPidProc);
    }

    @WorkerThread
    public UidJiffiesSnapshot currentUidJiffiesSnapshot() {
        return UidJiffiesSnapshot.of(mCore.getContext(), mCore.getConfig());
    }

    @AnyThread
    public void currentJiffiesSnapshot(@NonNull final Callback<JiffiesSnapshot> callback) {
        mCore.getHandler().post(new Runnable() {
            @Override
            public void run() {
                callback.onGetJiffies(currentJiffiesSnapshot());
            }
        });
    }

    @AnyThread
    public void currentJiffiesSnapshot(final int pid, @NonNull final Callback<JiffiesSnapshot> callback) {
        mCore.getHandler().post(new Runnable() {
            @Override
            public void run() {
                callback.onGetJiffies(currentJiffiesSnapshot(pid));
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

        static ProcessInfo getProcessInfo(int pid) {
            if (pid == Process.myPid()) {
                return getProcessInfo();
            }
            ProcessInfo processInfo = new ProcessInfo();
            processInfo.pid = pid;
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
        public boolean isNewAdded;
        public String name;
        public DigitEntry<Long> totalJiffies;
        public ListEntry<ThreadJiffiesSnapshot> threadEntries;
        public DigitEntry<Integer> threadNum;
        public ListEntry<ThreadJiffiesSnapshot> deadThreadEntries;

        private JiffiesSnapshot() {
            isNewAdded = false;
            deadThreadEntries = ListEntry.ofEmpty();
        }

        @Override
        public Delta<JiffiesSnapshot> diff(JiffiesSnapshot bgn) {
            return new Delta<JiffiesSnapshot>(bgn, this) {
                @Override
                protected JiffiesSnapshot computeDelta() {
                    JiffiesSnapshot delta = new JiffiesSnapshot();
                    delta.pid = end.pid;
                    delta.isNewAdded = end.isNewAdded;
                    delta.name = end.name;
                    delta.totalJiffies = Differ.DigitDiffer.globalDiff(bgn.totalJiffies, end.totalJiffies);
                    delta.threadNum = Differ.DigitDiffer.globalDiff(bgn.threadNum, end.threadNum);
                    delta.threadEntries = ListEntry.ofEmpty();

                    // for Existing threads
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
                                if (!isNewAdded || !sSkipNewAdded) {
                                    // Skip new added tid for now
                                    deltaThreadEntries.add(deltaThreadJiffies);
                                }
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

                    // for Dead threads
                    if (bgn.threadEntries.getList().size() > 0) {
                        List<ThreadJiffiesSnapshot> deadThreadEntries = Collections.emptyList();
                        for (ThreadJiffiesSnapshot bgn : bgn.threadEntries.getList()) {
                            boolean isDead = true;
                            for (ThreadJiffiesSnapshot exist : delta.threadEntries.getList()) {
                                if (exist.tid == bgn.tid) {
                                    isDead = false;
                                    break;
                                }
                            }
                            if (isDead) {
                                if (deadThreadEntries.isEmpty()) {
                                    deadThreadEntries = new ArrayList<>();
                                }
                                deadThreadEntries.add(bgn);
                            }
                        }
                        if (!deadThreadEntries.isEmpty()) {
                            delta.deadThreadEntries = ListEntry.of(deadThreadEntries);
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
            @Nullable
            public String stack;

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
        @Nullable private Handler mWatchHandler;

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
            synchronized (mWatchingThreads) {
                if (duringMillis <= 5 * 60 * 1000L) {
                    if (mWatchHandler != null) {
                        mWatchHandler.postDelayed(this, setNext(5 * 60 * 1000L));
                    }
                } else if (duringMillis <= 10 * 60 * 1000L) {
                    if (mWatchHandler != null) {
                        mWatchHandler.postDelayed(this, setNext(10 * 60 * 1000L));
                    }
                } else {
                    // done
                    synchronized (mWatchingThreads) {
                        mWatchingThreads.clear();
                    }
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
                    HandlerThread handlerThread = new HandlerThread("matrix_watchdog");
                    handlerThread.start();
                    mWatchHandler = new Handler(handlerThread.getLooper());
                    mWatchHandler.postDelayed(this, reset());
                }
            }
        }

        void stop() {
            synchronized (mWatchingThreads) {
                if (mWatchHandler != null) {
                    mWatchHandler.removeCallbacks(this);
                    mWatchHandler.getLooper().quit();
                    mWatchHandler = null;
                }
            }
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

    public static class UidJiffiesSnapshot extends Snapshot<UidJiffiesSnapshot> {
        public static UidJiffiesSnapshot of(Context context, BatteryMonitorConfig config) {
            UidJiffiesSnapshot curr = new UidJiffiesSnapshot();
            List<Pair<Integer, String>> procList = TopThreadFeature.getProcList(context);
            curr.pidCurrJiffiesList = new ArrayList<>(procList.size());
            long sum = 0;
            MatrixLog.i(TAG, "currProcList: " + procList);
            for (Pair<Integer, String> item : procList) {
                //noinspection ConstantConditions
                int pid = item.first;
                String procName = String.valueOf(item.second);
                if (ProcStatUtil.exists(pid)) {
                    MatrixLog.i(TAG, "proc: " + pid);
                    JiffiesSnapshot snapshot = JiffiesSnapshot.currentJiffiesSnapshot(ProcessInfo.getProcessInfo(pid), config.isStatPidProc);
                    snapshot.name = TopThreadIndicator.getProcSuffix(procName);
                    sum += snapshot.totalJiffies.get();
                    curr.pidCurrJiffiesList.add(snapshot);
                } else {
                    if (config.ipcJiffiesCollector != null) {
                        IpcJiffies.IpcProcessJiffies ipcProcessJiffies = config.ipcJiffiesCollector.apply(item);
                        if (ipcProcessJiffies != null) {
                            MatrixLog.i(TAG, "ipc: " + pid);
                            JiffiesSnapshot snapshot = IpcJiffies.toLocal(ipcProcessJiffies);
                            snapshot.name = TopThreadIndicator.getProcSuffix(procName);
                            sum += snapshot.totalJiffies.get();
                            curr.pidCurrJiffiesList.add(snapshot);
                            continue;
                        }
                    }
                    MatrixLog.i(TAG, "skip: " + pid);
                }
            }
            curr.totalUidJiffies = DigitEntry.of(sum);
            return curr;
        }

        public DigitEntry<Long> totalUidJiffies = DigitEntry.of(0L);
        public List<JiffiesSnapshot> pidCurrJiffiesList = Collections.emptyList();
        public List<Delta<JiffiesSnapshot>> pidDeltaJiffiesList = Collections.emptyList();

        @Override
        public Delta<UidJiffiesSnapshot> diff(UidJiffiesSnapshot bgn) {
            return new Delta<UidJiffiesSnapshot>(bgn, this) {
                @Override
                protected UidJiffiesSnapshot computeDelta() {
                    UidJiffiesSnapshot delta = new UidJiffiesSnapshot();
                    delta.totalUidJiffies = Differ.DigitDiffer.globalDiff(bgn.totalUidJiffies, end.totalUidJiffies);
                    if (end.pidCurrJiffiesList.size() > 0) {
                        delta.pidDeltaJiffiesList = new ArrayList<>();
                        for (JiffiesSnapshot end : end.pidCurrJiffiesList) {
                            JiffiesSnapshot last = null;
                            for (JiffiesSnapshot bgn : bgn.pidCurrJiffiesList) {
                                if (bgn.pid == end.pid) {
                                    last = bgn;
                                    break;
                                }
                            }
                            if (last == null) {
                                // newAdded Pid
                                end.isNewAdded = true;
                                JiffiesSnapshot empty = new JiffiesSnapshot();
                                empty.pid = end.pid;
                                empty.name = end.name;
                                empty.totalJiffies = DigitEntry.of(0L);
                                empty.threadEntries = ListEntry.ofEmpty();
                                empty.threadNum = DigitEntry.of(0);
                                last = empty;
                            }
                            if (!end.isNewAdded || !sSkipNewAdded) {
                                // Skip new added pid for now
                                Delta<JiffiesSnapshot> deltaPidJiffies = end.diff(last);
                                delta.pidDeltaJiffiesList.add(deltaPidJiffies);
                            }
                        }

                        Collections.sort(delta.pidDeltaJiffiesList, new Comparator<Delta<JiffiesSnapshot>>() {
                            @Override
                            public int compare(Delta<JiffiesSnapshot> o1, Delta<JiffiesSnapshot> o2) {
                                long minus = o1.dlt.totalJiffies.get() - o2.dlt.totalJiffies.get();
                                if (minus == 0) return 0;
                                if (minus > 0) return -1;
                                return 1;
                            }
                        });
                    }
                    return delta;
                }
            };
        }


        public static final class IpcJiffies {
            public static IpcProcessJiffies toIpc(JiffiesSnapshot local) {
                IpcProcessJiffies ipc = new IpcProcessJiffies();
                ipc.pid = local.pid;
                ipc.name = local.name;
                ipc.threadNum = local.threadNum.get();
                ipc.totalJiffies = local.totalJiffies.get();
                ipc.threadJiffyList = new ArrayList<>(local.threadEntries.getList().size());
                for (JiffiesSnapshot.ThreadJiffiesSnapshot item : local.threadEntries.getList()) {
                    ipc.threadJiffyList.add(toIpc(item));
                }
                return ipc;
            }

            public static IpcProcessJiffies.IpcThreadJiffies toIpc(JiffiesSnapshot.ThreadJiffiesSnapshot local) {
                IpcProcessJiffies.IpcThreadJiffies ipc = new IpcProcessJiffies.IpcThreadJiffies();
                ipc.tid = local.tid;
                ipc.name = local.name;
                ipc.stat = local.stat;
                ipc.jiffies = local.get();
                return ipc;
            }

            public static JiffiesSnapshot toLocal(IpcProcessJiffies ipc) {
                JiffiesSnapshot local = new JiffiesSnapshot();
                local.pid = ipc.pid;
                local.name = ipc.name;
                local.totalJiffies = DigitEntry.of(ipc.totalJiffies);
                List<JiffiesSnapshot.ThreadJiffiesSnapshot> threadJiffiesList = Collections.emptyList();
                if (!ipc.threadJiffyList.isEmpty()) {
                    threadJiffiesList = new ArrayList<>(ipc.threadJiffyList.size());
                    for (IpcProcessJiffies.IpcThreadJiffies item : ipc.threadJiffyList) {
                        JiffiesSnapshot.ThreadJiffiesSnapshot threadJiffies = toLocal(item);
                        threadJiffiesList.add(threadJiffies);
                    }
                }
                local.threadEntries = ListEntry.of(threadJiffiesList);
                local.threadNum = DigitEntry.of(threadJiffiesList.size());
                return local;
            }

            public static JiffiesSnapshot.ThreadJiffiesSnapshot toLocal(IpcProcessJiffies.IpcThreadJiffies ipc) {
                JiffiesSnapshot.ThreadJiffiesSnapshot local = new JiffiesSnapshot.ThreadJiffiesSnapshot(ipc.jiffies);
                local.name = ipc.name;
                local.stat = ipc.stat;
                local.tid = ipc.tid;
                local.isNewAdded = true;
                return local;
            }

            public static class IpcProcessJiffies implements Parcelable {
                public int pid;
                public String name;
                public long totalJiffies;
                public int threadNum;
                public List<IpcThreadJiffies> threadJiffyList;

                protected IpcProcessJiffies(Parcel in) {
                    pid = in.readInt();
                    name = in.readString();
                    totalJiffies = in.readLong();
                    threadNum = in.readInt();
                    threadJiffyList = in.createTypedArrayList(IpcThreadJiffies.CREATOR);
                }

                public static final Creator<IpcProcessJiffies> CREATOR = new Creator<IpcProcessJiffies>() {
                    @Override
                    public IpcProcessJiffies createFromParcel(Parcel in) {
                        return new IpcProcessJiffies(in);
                    }
                    @Override
                    public IpcProcessJiffies[] newArray(int size) {
                        return new IpcProcessJiffies[size];
                    }
                };

                public IpcProcessJiffies() {
                }

                @Override
                public int describeContents() {
                    return 0;
                }

                @Override
                public void writeToParcel(Parcel dest, int flags) {
                    dest.writeInt(pid);
                    dest.writeString(name);
                    dest.writeLong(totalJiffies);
                    dest.writeInt(threadNum);
                    dest.writeTypedList(threadJiffyList);
                }

                public static class IpcThreadJiffies implements Parcelable {
                    public int tid;
                    public String name;
                    public String stat;
                    public long jiffies;

                    protected IpcThreadJiffies(Parcel in) {
                        tid = in.readInt();
                        name = in.readString();
                        stat = in.readString();
                        jiffies = in.readLong();
                    }

                    public static final Creator<IpcThreadJiffies> CREATOR = new Creator<IpcThreadJiffies>() {
                        @Override
                        public IpcThreadJiffies createFromParcel(Parcel in) {
                            return new IpcThreadJiffies(in);
                        }
                        @Override
                        public IpcThreadJiffies[] newArray(int size) {
                            return new IpcThreadJiffies[size];
                        }
                    };

                    public IpcThreadJiffies() {
                    }

                    @Override
                    public int describeContents() {
                        return 0;
                    }

                    @Override
                    public void writeToParcel(Parcel dest, int flags) {
                        dest.writeInt(tid);
                        dest.writeString(name);
                        dest.writeString(stat);
                        dest.writeLong(jiffies);
                    }
                }
            }
        }
    }
}
