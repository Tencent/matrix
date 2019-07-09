package com.tencent.matrix.batterycanary.monitor.plugin;

import android.os.Handler;
import android.os.Message;
import android.os.Process;
import android.os.SystemClock;
import android.util.SparseArray;

import com.tencent.matrix.batterycanary.monitor.BatteryMonitor;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixUtil;

import java.io.File;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Objects;
import java.util.Set;

public class JiffiesMonitorPlugin implements IBatteryMonitorPlugin, Handler.Callback {

    private static final String TAG = "Matrix.JiffiesMonitorPlugin";
    private static final long WAIT_TIME = 2 * 60 * 1000;
    private static final int MSG_ID_JIFFIES_START = 0x1;
    private static final int MSG_ID_JIFFIES_END = 0x2;
    private Handler handler;
    private ProcessInfo lastProcessInfo = null;
    private BatteryMonitor monitor;

    @Override
    public void onInstall(BatteryMonitor monitor) {
        this.monitor = monitor;
        handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper(), this);
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
        if (!isForeground) {
            Message message = Message.obtain(handler);
            message.what = MSG_ID_JIFFIES_START;
            message.arg1 = (int) (SystemClock.uptimeMillis() / 1000);
            message.arg2 = (int) (System.currentTimeMillis() / 1000);
            handler.sendMessageDelayed(message, WAIT_TIME);
        } else if (!handler.hasMessages(MSG_ID_JIFFIES_START)) {
            Message message = Message.obtain(handler);
            message.what = MSG_ID_JIFFIES_END;
            message.arg1 = (int) (SystemClock.uptimeMillis() / 1000);
            message.arg2 = (int) (System.currentTimeMillis() / 1000);
            handler.sendMessageAtFrontOfQueue(message);
        }
    }


    @Override
    public boolean handleMessage(Message msg) {
        if (msg.what == MSG_ID_JIFFIES_START) {
            ProcessInfo processInfo = getProcessInfo();
            processInfo.threadInfo.addAll(getThreadsInfo(processInfo.pid));
            processInfo.upTime = SystemClock.uptimeMillis();
            processInfo.time = System.currentTimeMillis();
            lastProcessInfo = processInfo;
            return true;
        } else if (msg.what == MSG_ID_JIFFIES_END) {
            ProcessInfo processInfo = getProcessInfo();
            processInfo.threadInfo.addAll(getThreadsInfo(processInfo.pid));
            processInfo.upTime = SystemClock.uptimeMillis();
            processInfo.time = System.currentTimeMillis();
            Objects.requireNonNull(lastProcessInfo, "error! why lastProcessInfo is null!");

            JiffiesResult result = calculateDiff(lastProcessInfo, processInfo);
            printResult(result);
            if (null != monitor.getConfig().printer) {
                monitor.getConfig().printer.onJiffies(result);
            }
            lastProcessInfo = null;
            return true;
        }
        return false;
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
            jiffiesStart += info.jiffies;
            ThreadResult threadResult = array.get(info.tid);
            if (null == threadResult) {
                array.put(info.tid, ThreadResult.obtain(info, 3));
            } else {
                threadResult.threadState = 2;
                threadResult.jiffiesDiff = threadResult.threadInfo.jiffies - info.jiffies;
                threadResult.threadInfo = null; // reset
            }
        }

        JiffiesResult result = new JiffiesResult();
        result.jiffiesDiff = endProcessInfo.jiffies - startProcessInfo.jiffies;
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
        long jiffiesDiff;
        long jiffiesDiff2;
        long timeDiff;
        long upTimeDiff;
        LinkedList<ThreadResult> threadResults = new LinkedList<>();
    }

    static class ThreadResult implements Comparable<ThreadResult> {
        ThreadInfo threadInfo;
        long jiffiesDiff;
        int threadState = 1; // 1 new, 2 keeping, 3 quit

        private ThreadResult(ThreadInfo threadInfo, int threadState) {
            this.threadInfo = threadInfo;
            this.threadState = threadState;
        }

        static ThreadResult obtain(ThreadInfo threadInfo, int state) {
            return new ThreadResult(threadInfo, state);
        }

        @Override
        public int compareTo(ThreadResult o) {
            return Long.compare(o.jiffiesDiff, jiffiesDiff);
        }
    }

    private static long getTotalJiffiesByStat(int tid, int pid) {
        String stat;
        if (tid == -1) {
            stat = String.format("/proc/%s/stat", pid);
        } else {
            stat = String.format("/proc/%s/task/%s/stat", pid, tid);
        }

        try {
            String task_str = MatrixUtil.getStringFromFile(stat);
            if (null == task_str) {
                return -1L;
            }
            String[] info = task_str.split(" ");
            long utime = Long.parseLong(info[13]);
            long stime = Long.parseLong(info[14]);
            long cutime = Long.parseLong(info[15]);
            long cstime = Long.parseLong(info[16]);
            return utime + stime + cutime + cstime;

        } catch (Exception e) {
            return -1L;
        }
    }

    private ProcessInfo getProcessInfo() {
        ProcessInfo processInfo = new ProcessInfo();
        processInfo.pid = Process.myPid();
        String processDir = "/proc/";
        File dirFile = new File(processDir);
        if (dirFile.isDirectory()) {
            File[] array = dirFile.listFiles();
            if (null == array) {
                return processInfo;
            }
            for (File file : array) {
                try {
                    int pid = Integer.parseInt(file.getName().trim());
                    if (Process.myPid() == pid) {
                        String content = MatrixUtil.getStringFromFile(processDir + file.getName() + "/stat");
                        if (null != content) {
                            String[] args = content.replaceAll("\n", "").split(" ");
                            processInfo.name = args[1].replace("(", "").replace(")", "");
                            processInfo.jiffies = getTotalJiffiesByStat(-1, pid);

                        }
                    }
                } catch (Exception e) {
                }
            }
        }
        return processInfo;
    }

    private Set<ThreadInfo> getThreadsInfo(int pid) {
        Set<ThreadInfo> set = new HashSet<>();
        String threadDir = String.format("/proc/%s/task/", pid);
        File dirFile = new File(threadDir);
        if (dirFile.isDirectory()) {
            File[] array = dirFile.listFiles();
            if (null == array) {
                return set;
            }
            for (File file : array) {
                try {
                    String content = MatrixUtil.getStringFromFile(threadDir + file.getName() + "/stat");
                    if (null != content) {
                        String[] args = content.replaceAll("\n", "").split(" ");
                        ThreadInfo threadInfo = new ThreadInfo();
                        threadInfo.tid = Integer.parseInt(args[0]);
                        threadInfo.name = args[1].replace("(", "").replace(")", "");
                        if (threadInfo.tid == pid) {
                            threadInfo.name = "main";
                        } else if (threadInfo.name.isEmpty()) {
                            threadInfo.name = "null";
                        }
                        threadInfo.jiffies = getTotalJiffiesByStat(threadInfo.tid, pid);
                        set.add(threadInfo);
                    }
                } catch (Exception e) {
                }
            }

        }
        return set;
    }


    private class ProcessInfo {
        int pid;
        String name;
        long jiffies;
        long time;
        long upTime;
        LinkedList<ThreadInfo> threadInfo = new LinkedList<>();

        @Override
        public String toString() {
            return "process:" + name + "(" + pid + ") " + jiffies + " thread size:" + threadInfo.size();
        }
    }

    private class ThreadInfo {
        int tid;
        String name;
        long jiffies;

        @Override
        public String toString() {
            return "thread:" + name + "(" + tid + ") " + jiffies;
        }
    }

}
