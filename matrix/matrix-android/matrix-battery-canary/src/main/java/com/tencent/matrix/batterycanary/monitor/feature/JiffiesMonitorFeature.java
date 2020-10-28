package com.tencent.matrix.batterycanary.monitor.feature;

import android.os.Handler;
import android.os.Message;
import android.os.Process;
import android.os.SystemClock;
import android.support.annotation.NonNull;
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
import java.util.Set;

public class JiffiesMonitorFeature implements MonitorFeature, Handler.Callback {
    private static final String TAG = "Matrix.JiffiesMonitorPlugin";

    public interface JiffiesListener {
        void onTraceBegin();
        void onTraceEnd();
        void onJiffies(JiffiesMonitorFeature.JiffiesResult result);
    }

    private static long WAIT_TIME;
    private static long LOOP_TIME = 15 * 60 * 1000;
    private static final int MSG_ID_JIFFIES_START = 0x1;
    private static final int MSG_ID_JIFFIES_END = 0x2;
    private Handler handler;
    private ProcessInfo lastProcessInfo = null;
    private BatteryMonitorCore monitor;
    private LoopCheckRunnable foregroundLoopCheckRunnable = new LoopCheckRunnable();
    private static byte[] sBuffer = new byte[2 * 1024];

    private JiffiesListener getListener() {
        return monitor;
    }

    @Override
    public void configure(BatteryMonitorCore monitor) {
        this.monitor = monitor;
        handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper(), this);
        WAIT_TIME = monitor.getConfig().greyTime;
        LOOP_TIME = monitor.getConfig().foregroundLoopCheckTime;
    }

    @Override
    public void onTurnOn() {
    }

    @Override
    public void onTurnOff() {
        handler.removeCallbacksAndMessages(null);
    }

    @Override
    public void onAppForeground(boolean isForeground) {
        if (!Matrix.isInstalled()) {
            MatrixLog.e(TAG, "Matrix was not installed yet, just ignore the event");
            return;
        }

        if (!isForeground) {
            handler.removeCallbacksAndMessages(null);
            Message message = Message.obtain(handler);
            message.what = MSG_ID_JIFFIES_START;
            handler.sendMessageDelayed(message, WAIT_TIME);
        } else if (!handler.hasMessages(MSG_ID_JIFFIES_START)) {
            Message message = Message.obtain(handler);
            message.what = MSG_ID_JIFFIES_END;
            handler.sendMessageAtFrontOfQueue(message);
        }

        if (isForeground) {
            handler.removeCallbacks(foregroundLoopCheckRunnable);
            if (monitor.getConfig().isForegroundModeEnabled) {
                foregroundLoopCheckRunnable.lastWhat = MSG_ID_JIFFIES_START;
                handler.post(foregroundLoopCheckRunnable);
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
            ProcessInfo processInfo = new ProcessInfo();
            processInfo.pid = Process.myPid();
            processInfo.name = Matrix.isInstalled() ? MatrixUtil.getProcessName(Matrix.with().getApplication()) : "default";
            processInfo.threadInfo.addAll(getThreadsInfo(processInfo.pid));
            processInfo.upTime = SystemClock.uptimeMillis();
            processInfo.time = System.currentTimeMillis();
            lastProcessInfo = processInfo;
            if (null != getListener()) {
                getListener().onTraceBegin();
            }
            return true;
        } else if (msg.what == MSG_ID_JIFFIES_END) {
            if (null == lastProcessInfo) {
                return true;
            }
            if (null != getListener()) {
                getListener().onTraceEnd();
            }
            ProcessInfo processInfo = new ProcessInfo();
            processInfo.pid = Process.myPid();
            processInfo.name = Matrix.isInstalled() ? MatrixUtil.getProcessName(Matrix.with().getApplication()) : "default";
            processInfo.threadInfo.addAll(getThreadsInfo(processInfo.pid));
            processInfo.upTime = SystemClock.uptimeMillis();
            processInfo.time = System.currentTimeMillis();
            JiffiesResult result = calculateDiff(lastProcessInfo, processInfo);
            result.isForeground = msg.arg1 == 1;
            printResult(result);
            if (null != getListener()) {
                getListener().onJiffies(result);
            }
            lastProcessInfo = null;
            return true;
        }
        return false;
    }

    private class LoopCheckRunnable implements Runnable {

        int lastWhat = MSG_ID_JIFFIES_START;

        @Override
        public void run() {
            Message message = Message.obtain(handler);
            message.what = lastWhat;
            message.arg1 = 1;
            handler.sendMessageAtFrontOfQueue(message);
            lastWhat = (lastWhat == MSG_ID_JIFFIES_END ? MSG_ID_JIFFIES_START : MSG_ID_JIFFIES_END);
            handler.postDelayed(this, LOOP_TIME);
        }
    }

    private void printResult(JiffiesResult result) {
    }


    private JiffiesResult calculateDiff(ProcessInfo startProcessInfo, ProcessInfo endProcessInfo) {
        long jiffiesStart = 0;
        long jiffiesEnd = 0;

        SparseArray<ThreadResult> array = new SparseArray<>();
        for (ThreadInfo info : endProcessInfo.threadInfo) {
            jiffiesEnd += info.jiffies;
            ThreadResult threadResult = ThreadResult.obtain(info, 1);
            threadResult.jiffiesDiff = info.jiffies;
            array.put(info.tid, threadResult);
        }
        for (ThreadInfo info : startProcessInfo.threadInfo) {
            ThreadResult threadResult = array.get(info.tid);
            if (null == threadResult) {
                array.put(info.tid, ThreadResult.obtain(info, 3));
            } else {
                jiffiesStart += info.jiffies;
                threadResult.threadState = 2;
                threadResult.jiffiesDiff = threadResult.threadInfo.jiffies - info.jiffies;
            }
        }

        JiffiesResult result = new JiffiesResult();
        result.jiffiesDiff2 = jiffiesEnd - jiffiesStart;
        result.timeDiff = endProcessInfo.time - startProcessInfo.time;
        result.upTimeDiff = endProcessInfo.upTime - startProcessInfo.upTime;
        for (int i = 0; i < array.size(); i++) {
            result.threadResults.add(array.valueAt(i));
        }
        Collections.sort(result.threadResults);
        return result;
    }

    public static class JiffiesResult {
        public long jiffiesDiff2;
        public long timeDiff;
        public long upTimeDiff;
        public boolean isForeground;
        public LinkedList<ThreadResult> threadResults = new LinkedList<>();
    }

    public static class ThreadResult implements Comparable<ThreadResult> {
        public ThreadInfo threadInfo;
        public long jiffiesDiff;
        int threadState = 1; // 1 new, 2 keeping, 3 quit

        private ThreadResult(ThreadInfo threadInfo, int threadState) {
            this.threadInfo = threadInfo;
            this.threadState = threadState;
        }

        static ThreadResult obtain(ThreadInfo threadInfo, int state) {
            return new ThreadResult(threadInfo, state);
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
            ss.append("(").append(state).append(")").append(threadInfo.name).append("(").append(threadInfo.tid).append(")\t").append(jiffiesDiff).append(" jiffies");
            return ss.toString();
        }

        @Override
        public int compareTo(ThreadResult o) {
            return Long.compare(o.jiffiesDiff, jiffiesDiff);
        }
    }

    private Set<ThreadInfo> getThreadsInfo(int pid) {
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

                    parseJiffiesInfo(threadDir + file.getName() + "/stat", threadInfo);

                    if (threadInfo.tid == pid) {
                        threadInfo.name =  "main";
                    } else if (TextUtils.isEmpty(threadInfo.name)) {
                        threadInfo.name = "null";
                    }
                } catch (Exception e) {
                } finally {
                    if (null != threadInfo) {
                        set.add(threadInfo);
                    }
                }

            }
        }

        return set;
    }

    private static void parseJiffiesInfo(String path, ThreadInfo info) {
        final int readBytes = readProcStat(path, sBuffer);

//        MatrixLog.d(TAG, "%d: %s", readBytes, new String(sBuffer, 0, readBytes));

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

//                    MatrixLog.d(TAG, "read name = %s, begin = %d, length = %d", info.name, begin, length);

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

//                    MatrixLog.d(TAG, "read jiffies = %s", num);

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


    private class ProcessInfo {
        int pid;
        String name;
        long time;
        long upTime;
        LinkedList<ThreadInfo> threadInfo = new LinkedList<>();

        @Override
        public String toString() {
            return "process:" + name + "(" + pid + ") " + "thread size:" + threadInfo.size();
        }
    }

    public class ThreadInfo {
        public int tid;
        public String name;
        public long jiffies;

        @Override
        public String toString() {
            return "thread:" + name + "(" + tid + ") " + jiffies;
        }
    }

}
