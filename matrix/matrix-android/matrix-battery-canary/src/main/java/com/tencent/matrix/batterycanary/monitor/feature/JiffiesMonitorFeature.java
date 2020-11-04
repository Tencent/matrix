package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Handler;
import android.os.Message;
import android.os.Process;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;
import android.util.SparseArray;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCore;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

@SuppressWarnings("NotNullFieldNotInitialized")
public class JiffiesMonitorFeature implements MonitorFeature, Handler.Callback {
    private static final String TAG = "Matrix.monitor.JiffiesMonitorFeature";
    private boolean isForegroundModeEnabled;

    public interface JiffiesListener {
        void onTraceBegin();
        void onTraceEnd();
        void onJiffies(JiffiesMonitorFeature.JiffiesResult result);
    }

    private static final int MSG_ID_JIFFIES_START = 0x1;
    private static final int MSG_ID_JIFFIES_END = 0x2;
    private static long LAZY_TIME;
    private static long LOOP_TIME;
    @NonNull private BatteryMonitorCore monitor;
    @NonNull private Handler handler;
    @Nullable private ForegroundLoopCheckTask foregroundLoopCheckTask;
    private ProcessInfo lastProcessInfo = null;

    private JiffiesListener getListener() {
        return monitor;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        MatrixLog.i(TAG, "#configure monitor feature");
        this.monitor = monitor;
        handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper(), this);
        enableForegroundLoopCheck(monitor.getConfig().isForegroundModeEnabled);
        LAZY_TIME = monitor.getConfig().greyTime;
        LOOP_TIME = monitor.getConfig().foregroundLoopCheckTime;
    }

    @VisibleForTesting
    public void enableForegroundLoopCheck(boolean bool) {
        isForegroundModeEnabled = bool;
        if (isForegroundModeEnabled) {
            foregroundLoopCheckTask = new ForegroundLoopCheckTask();
        }
    }

    @Override
    public void onTurnOn() {
        MatrixLog.i(TAG, "#onTurnOn");
    }

    @Override
    public void onTurnOff() {
        MatrixLog.i(TAG, "#onTurnOff");
        handler.removeCallbacksAndMessages(null);
    }

    @Override
    public void onForeground(boolean isForeground) {
        MatrixLog.i(TAG, "#onAppForeground, bool = " + isForeground);
        if (!Matrix.isInstalled()) {
            MatrixLog.e(TAG, "Matrix was not installed yet, just ignore the event");
            return;
        }

        if (!isForeground) {
            // back:
            // 1. remove all checks
            handler.removeCallbacksAndMessages(null);
            // 2. start background jiffies check
            Message message = Message.obtain(handler);
            message.what = MSG_ID_JIFFIES_START;
            handler.sendMessageDelayed(message, LAZY_TIME);
        } else if (!handler.hasMessages(MSG_ID_JIFFIES_START)) {
            // fore:
            // 1. finish background jiffies check
            Message message = Message.obtain(handler);
            message.what = MSG_ID_JIFFIES_END;
            handler.sendMessageAtFrontOfQueue(message);
            // 2. start foreground jiffies loop check
            if (isForegroundModeEnabled && foregroundLoopCheckTask != null) {
                handler.removeCallbacks(foregroundLoopCheckTask);
                foregroundLoopCheckTask.lastWhat = MSG_ID_JIFFIES_START;
                handler.post(foregroundLoopCheckTask);
            }
        }
    }

    @Override
    public int weight() {
        return Integer.MAX_VALUE;
    }


    @Override
    public boolean handleMessage(Message msg) {
        if (msg.what == MSG_ID_JIFFIES_START) {
            lastProcessInfo = currentProcessInfo();
            getListener().onTraceBegin();
            return true;
        }
        if (msg.what == MSG_ID_JIFFIES_END) {
            getListener().onTraceEnd();
            JiffiesResult result = currentJiffies();
            if (result != null) {
                result.status = msg.arg1 == 1 ? "fg" : "bg";
                getListener().onJiffies(result);
            }
            lastProcessInfo = null;
            return true;
        }
        return false;
    }

    private ProcessInfo currentProcessInfo() {
        return ProcessInfo.getProcessInfo();
    }

    @WorkerThread
    @Nullable
    public JiffiesResult currentJiffies() {
        if (null != lastProcessInfo) {
            ProcessInfo processInfo = currentProcessInfo();
            return JiffiesResult.calculateJiffiesDiff(lastProcessInfo, processInfo);
        }
        return null;
    }

    private class ForegroundLoopCheckTask implements Runnable {
        int lastWhat = MSG_ID_JIFFIES_START;
        @Override
        public void run() {
            if (isForegroundModeEnabled) {
                Message message = Message.obtain(handler);
                message.what = lastWhat;
                message.arg1 = 1;
                handler.sendMessageAtFrontOfQueue(message);
                lastWhat = (lastWhat == MSG_ID_JIFFIES_END ? MSG_ID_JIFFIES_START : MSG_ID_JIFFIES_END);
                handler.postDelayed(this, LOOP_TIME);
            }
        }
    }

    private static class ProcessInfo {
        int pid;
        String name;
        long time;
        long upTime;
        LinkedList<ThreadInfo> threadInfo = new LinkedList<>();

        private static ProcessInfo getProcessInfo() {
            ProcessInfo processInfo = new ProcessInfo();
            processInfo.pid = Process.myPid();
            processInfo.name = Matrix.isInstalled() ? MatrixUtil.getProcessName(Matrix.with().getApplication()) : "default";
            processInfo.threadInfo.addAll(ThreadInfo.getThreadsInfo(processInfo.pid));
            processInfo.upTime = SystemClock.uptimeMillis();
            processInfo.time = System.currentTimeMillis();
            return processInfo;
        }

        @Override
        public String toString() {
            return "process:" + name + "(" + pid + ") " + "thread size:" + threadInfo.size();
        }

        public static class ThreadInfo {
            public int tid;
            public String name;
            public long jiffies;

            private static Set<ThreadInfo> getThreadsInfo(int pid) {
                Set<ThreadInfo> set = new HashSet<>();
                String threadDir = String.format("/proc/%s/task/", pid);
                File dirFile = new File(threadDir);
                if (dirFile.isDirectory()) {
                    File[] subDirs = dirFile.listFiles();
                    if (null == subDirs) {
                        return set;
                    }

                    for (File file : subDirs) {
                        if (!file.isDirectory()) {
                            continue;
                        }

                        ThreadInfo threadInfo = null;

                        try {
                            threadInfo = new ThreadInfo();
                            threadInfo.tid = Integer.parseInt(file.getName());

                            JiffiesResult.ThreadJiffies.parseThreadJiffiesInfo(threadDir + file.getName() + "/stat", threadInfo);

                            if (threadInfo.tid == pid) {
                                threadInfo.name =  "main";
                            } else if (TextUtils.isEmpty(threadInfo.name)) {
                                threadInfo.name = "null";
                            }
                        } catch (Exception ignored) {
                        } finally {
                            if (null != threadInfo) {
                                set.add(threadInfo);
                            }
                        }

                    }
                }

                return set;
            }

            @Override
            public String toString() {
                return "thread:" + name + "(" + tid + ") " + jiffies;
            }
        }
    }

    public static class JiffiesResult {
        public long totalJiffiesDiff;
        public long timeDiff;
        public long upTimeDiff;
        public String status = "default";
        public final List<ThreadJiffies> threadJiffies = new LinkedList<>();

        private static JiffiesResult calculateJiffiesDiff(ProcessInfo startProcessInfo, ProcessInfo endProcessInfo) {
            long jiffiesStart = 0;
            long jiffiesEnd = 0;

            SparseArray<ThreadJiffies> array = new SparseArray<>();
            for (ProcessInfo.ThreadInfo info : endProcessInfo.threadInfo) {
                jiffiesEnd += info.jiffies;
                ThreadJiffies threadJiffies = ThreadJiffies.obtain(info, 1);
                threadJiffies.jiffiesDiff = info.jiffies;
                array.put(info.tid, threadJiffies);
            }
            for (ProcessInfo.ThreadInfo info : startProcessInfo.threadInfo) {
                ThreadJiffies threadJiffies = array.get(info.tid);
                if (null == threadJiffies) {
                    array.put(info.tid, ThreadJiffies.obtain(info, 3));
                } else {
                    jiffiesStart += info.jiffies;
                    threadJiffies.threadState = 2;
                    threadJiffies.jiffiesDiff = threadJiffies.threadInfo.jiffies - info.jiffies;
                }
            }

            JiffiesResult result = new JiffiesResult();
            result.totalJiffiesDiff = jiffiesEnd - jiffiesStart;
            result.timeDiff = endProcessInfo.time - startProcessInfo.time;
            result.upTimeDiff = endProcessInfo.upTime - startProcessInfo.upTime;
            for (int i = 0; i < array.size(); i++) {
                result.threadJiffies.add(array.valueAt(i));
            }
            Collections.sort(result.threadJiffies);
            return result;
        }

        public static class ThreadJiffies implements Comparable<ThreadJiffies> {
            private static byte[] sBuffer = new byte[2 * 1024];
            public ProcessInfo.ThreadInfo threadInfo;
            public long jiffiesDiff;
            int threadState = 1; // 1 new, 2 keeping, 3 quit

            private ThreadJiffies(ProcessInfo.ThreadInfo threadInfo, int threadState) {
                this.threadInfo = threadInfo;
                this.threadState = threadState;
            }

            static ThreadJiffies obtain(ProcessInfo.ThreadInfo threadInfo, int state) {
                return new ThreadJiffies(threadInfo, state);
            }

            private static void parseThreadJiffiesInfo(String path, ProcessInfo.ThreadInfo info) {
                final int readBytes = readProcStat(path, sBuffer);
                // MatrixLog.d(TAG, "%d: %s", readBytes, new String(sBuffer, 0, readBytes));
                for (int i = 0, spaceIdx = 0; i < readBytes;) {
                    if (Character.isSpaceChar(sBuffer[i])) {
                        spaceIdx++;
                        i++;
                        continue;
                    }

                    switch (spaceIdx) {
                        case 1: { // read thread name
                            int begin = i;
                            int length = 0;

                            for (;i < readBytes && !Character.isSpaceChar(sBuffer[i]); i++, length++);

                            if ('(' == sBuffer[begin]) {
                                begin++;
                                length--;
                            }

                            if (')' == sBuffer[begin + length - 1]) {
                                length--;
                            }

                            if (length > 0) {
                                info.name = new String(sBuffer, begin, length);
                            }

                            // MatrixLog.d(TAG, "read name = %s, begin = %d, length = %d", info.name, begin, length);
                            break;
                        }
                        case 13:
                        case 14:
                        case 15:
                        case 16: { // read jiffies
                            int begin = i;
                            int length = 0;

                            for (; i < readBytes && !Character.isSpaceChar(sBuffer[i]); i++, length++);

                            String num = new String(sBuffer, begin, length);
                            // MatrixLog.d(TAG, "read jiffies = %s", num);
                            info.jiffies += MatrixUtil.parseLong(num, 0);

                            break;
                        }
                        default:
                            i++;
                    }
                }
            }

            private static int readProcStat(String path, byte[] buffer) {
                int readBytes = -1;
                File file = new File(path);
                if (!file.exists()) {
                    return readBytes;
                }

                try (FileInputStream fis = new FileInputStream(file)) {
                    readBytes = fis.read(buffer);
                } catch (IOException e) {
                    MatrixLog.printErrStackTrace(TAG, e, "");
                    readBytes = -1;
                }

                return readBytes;
            }

            @NonNull
            @Override
            public String toString() {
                StringBuilder ss = new StringBuilder();
                String state;
                if (threadState == 1) {
                    state = "+";
                } else if (threadState == 2) {
                    state = "~";
                } else {
                    state = "-";
                }
                ss.append("(").append(state).append(")").append(threadInfo.name).append("(").append(threadInfo.tid).append(")\t").append(jiffiesDiff).append("\tjiffies");
                return ss.toString();
            }

            @Override
            public int compareTo(ThreadJiffies o) {
                return Long.compare(o.jiffiesDiff, jiffiesDiff);
            }
        }
    }
}
