package com.tencent.matrix.threadcanary;

import android.os.HandlerThread;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;

import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class ThreadWatcher extends Plugin {

    private static final String TAG = "Matrix.ThreadWatcher";
    private final ThreadConfig mThreadConfig;
    private DumpThreadJiffiesTask mCheckThreadTask;

    public ThreadWatcher(ThreadConfig config) {
        this.mThreadConfig = config;
    }


    private final IThreadFilter mThreadFilter = new IThreadFilter() {
        @Override
        public boolean isFilter(ThreadInfo threadInfo) {
            if (null == mThreadConfig.getFilterThreadSet()) {
                return false;
            }
            for (String s : mThreadConfig.getFilterThreadSet()) {
                if (threadInfo.name.contains(s)) {
                    return true;
                }
            }
            return false;
        }
    };


    @Override
    public void start() {
        super.start();
        MatrixHandlerThread.getDefaultMainHandler().post(new Runnable() {
            @Override
            public void run() {
                MatrixLog.i(TAG, "[start]");
                mCheckThreadTask = new DumpThreadJiffiesTask();
                MatrixHandlerThread.getDefaultHandler().post(mCheckThreadTask);
            }
        });
    }

    @Override
    public void stop() {
        super.stop();
        MatrixLog.i(TAG, "stop");
        MatrixHandlerThread.getDefaultHandler().removeCallbacks(mCheckThreadTask);
    }

    private static class ThreadInfo {
        String name;
        long tid;
        boolean isHandlerThread;
        String state;
        long jiffies;
        static HashMap<Long, Long> sBeginStackSizeMap = new HashMap<>();
        long bigStackSize;
        long bigVirtualSize; //Virtual memory size in bytes
        String stack;

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof ThreadInfo) {
                ThreadInfo info = (ThreadInfo) obj;
                return name.equals(info.name) && tid == info.tid;
            } else {
                return false;
            }
        }

        @Override
        public int hashCode() {
            return (int) tid;
        }

        @Override
        public String toString() {
            return String.format("%s %s %s %s %s %s %s", name, tid, state, jiffies, bigStackSize, bigVirtualSize, isHandlerThread);
        }
    }

    private final HashMap<Long, ThreadInfo> mDumpThreadInfoMap = new HashMap<>();
    private static boolean isTest = false;

    private class DumpThreadJiffiesTask implements Runnable {
        private final long mainTid = Process.myPid();

        private DumpThreadJiffiesTask() {
        }

        @Override
        public void run() {
            Log.i(TAG, "[DumpThreadJiffiesTask] RUN!");
            final Map<Long, ThreadInfo> appThreadsMap = getAppThreadsMap(mThreadFilter);
            Set<ThreadInfo> linuxThreads = getThreadsInfo(new IThreadInfoIterator() {
                @Override
                public void next(ThreadInfo threadInfo) {
                    ThreadInfo appThreadInfo = appThreadsMap.get(threadInfo.tid);
                    if (threadInfo.tid == mainTid) {
                        threadInfo.name = "main";
                    } else {
                        threadInfo.name = appThreadInfo.name.replaceAll("-?[0-9]\\d*", "?");
                    }
                    threadInfo.stack = appThreadInfo.stack;
                }
            }, new IThreadFilter() {
                @Override
                public boolean isFilter(ThreadInfo threadInfo) {
                    return !appThreadsMap.containsKey(threadInfo.tid) || mThreadFilter.isFilter(threadInfo);
                }
            });

            for (ThreadInfo threadInfo : linuxThreads) {
                if (mDumpThreadInfoMap.containsKey(threadInfo.tid)) {
                    ThreadInfo old = mDumpThreadInfoMap.get(threadInfo.tid);
                    threadInfo.bigVirtualSize = old.bigVirtualSize > threadInfo.bigVirtualSize ? old.bigVirtualSize : threadInfo.bigVirtualSize;
                    if (old.bigStackSize > threadInfo.bigStackSize) {
                        threadInfo.stack = old.stack;
                    }
                    threadInfo.bigStackSize = old.bigStackSize > threadInfo.bigStackSize ? old.bigStackSize : threadInfo.bigStackSize;
                }

                if (isTest) {
                    MatrixLog.i(TAG, "thread:%s", threadInfo);
                }

                mDumpThreadInfoMap.put(threadInfo.tid, threadInfo);
            }

            long time = SystemClock.uptimeMillis();
            if (time - mLastReportTime > mThreadConfig.getReportTime() && !mDumpThreadInfoMap.isEmpty()) {
                report();
                mDumpThreadInfoMap.clear();
            }
            MatrixHandlerThread.getDefaultHandler().postDelayed(this, mThreadConfig.getCheckTime());
        }
    }


    private final long mLastReportTime = SystemClock.uptimeMillis();

    private void report() {
        Issue issue = new Issue();
        issue.setTag(Constants.TAG_PLUGIN);
        JSONObject jsonObject = new JSONObject();
        issue.setContent(jsonObject);
        JSONArray listObj = new JSONArray();
        try {
            jsonObject.put(Constants.REPORT_KEY_THREAD_COUNT, mDumpThreadInfoMap.size());
            jsonObject.put(Constants.REPORT_KEY_THREAD_LIST, listObj);
            for (ThreadInfo info : mDumpThreadInfoMap.values()) {
                JSONObject infoJson = new JSONObject();
                infoJson.put(Constants.REPORT_KEY_THREAD_INFO_NAME, info.name);
                infoJson.put(Constants.REPORT_KEY_THREAD_INFO_TID, info.tid);
                infoJson.put(Constants.REPORT_KEY_THREAD_INFO_STATE, info.state);
                infoJson.put(Constants.REPORT_KEY_THREAD_INFO_JIFFIES, info.jiffies);
                infoJson.put(Constants.REPORT_KEY_THREAD_INFO_BIG_STACK_SIZE, info.bigStackSize);
                infoJson.put(Constants.REPORT_KEY_THREAD_INFO_BIG_VIRTUAL_SIZE, info.bigVirtualSize);
                listObj.put(infoJson);
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
        onDetectIssue(issue);
    }

    public interface IThreadFilter {
        boolean isFilter(ThreadInfo threadInfo);
    }

    private static Set<ThreadInfo> getAppThreads(IThreadFilter filter) {
        HashSet<ThreadInfo> set = new HashSet<>();
        final ThreadGroup threadGroup = Looper.getMainLooper().getThread().getThreadGroup();
        int estimatedSize = threadGroup.activeCount() * 2;
        Thread[] slackList = new Thread[estimatedSize];
        int actualSize = threadGroup.enumerate(slackList);
        for (int i = 0; i < actualSize; i++) {
            Thread thread = slackList[i];
            ThreadInfo threadInfo = new ThreadInfo();
            threadInfo.name = thread.getName();
            if (null != filter && filter.isFilter(threadInfo)) {
                continue;
            }
            if (thread instanceof HandlerThread) {
                HandlerThread handlerThread = (HandlerThread) thread;
                threadInfo.tid = handlerThread.getThreadId();
            } else {
                threadInfo.tid = thread.getId();
            }
            set.add(threadInfo);
        }
        return set;
    }

    private static HashMap<Long, ThreadInfo> getAppThreadsMap(IThreadFilter filter) {
        HashMap<Long, ThreadInfo> map = new HashMap<>();
        Map<Thread, StackTraceElement[]> stacks = Thread.getAllStackTraces();
        Set<Thread> set = stacks.keySet();
        for (Thread thread : set) {
            ThreadInfo threadInfo = new ThreadInfo();
            threadInfo.name = thread.getName();
            if (null != filter && filter.isFilter(threadInfo)) {
                continue;
            }
            threadInfo.stack = stackTraceToString(thread.getStackTrace());
            if (thread instanceof HandlerThread) {
                threadInfo.isHandlerThread = true;
                map.put((long) ((HandlerThread) thread).getThreadId(), threadInfo);
            } else {
                threadInfo.isHandlerThread = false;
                map.put(thread.getId(), threadInfo);
            }
        }
        return map;
    }

    public static String stackTraceToString(final StackTraceElement[] stackTrace) {

        if ((stackTrace == null) || (stackTrace.length < 4)) {
            return "";
        }

        StringBuilder t = new StringBuilder();

        for (int i = 3; i < stackTrace.length; i++) {
            t.append('[');
            t.append(stackTrace[i].getClassName());
            t.append(':');
            t.append(stackTrace[i].getMethodName());
            t.append("(" + stackTrace[i].getLineNumber() + ")]");
        }
        return t.toString();
    }


    private interface IThreadInfoIterator {
        void next(ThreadInfo info);
    }

    public static Set<ThreadInfo> getThreadsInfo(IThreadInfoIterator iterator, IThreadFilter filter) {
        Set<ThreadInfo> set = new HashSet<>();
        String threadDir = String.format("/proc/%s/task/", Process.myPid());
        File dirFile = new File(threadDir);
        if (dirFile.isDirectory()) {
            File[] array = dirFile.listFiles();
            for (File file : array) {
                try {
                    String content = getStringFromFile(threadDir + file.getName() + "/stat");
                    if (null != content) {
                        String[] args = content.replaceAll("\n", "").split(" ");
                        ThreadInfo threadInfo = new ThreadInfo();
                        threadInfo.tid = Long.parseLong(args[0]);
                        threadInfo.name = args[1].replace("(", "").replace(")", "");
                        threadInfo.state = args[2].replace("'", "");
                        threadInfo.jiffies = getJiffies(threadInfo.tid);
                        threadInfo.bigVirtualSize = Long.parseLong(args[23].trim());
                        long nowStackSize = Long.parseLong(args[28].trim());
                        if (threadInfo.sBeginStackSizeMap.containsKey(threadInfo.tid)) {
                            long begin = threadInfo.sBeginStackSizeMap.get(threadInfo.tid);
                            threadInfo.bigStackSize = Math.max(0, begin - nowStackSize);
                        } else {
                            threadInfo.sBeginStackSizeMap.put(threadInfo.tid, nowStackSize);
                            threadInfo.bigStackSize = 0;
                        }

                        if (!filter.isFilter(threadInfo)) {
                            set.add(threadInfo);
                            iterator.next(threadInfo);
                        }

                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
        return set;
    }

    public static long getTotalJiffies() {
        String threadStat = String.format("/proc/%s/task/%s/stat", Process.myPid(), Process.myTid());
        try {
            String content = getStringFromFile(threadStat);
            if (null == content) {
                return -1L;
            }
            String[] info = content.split(" ");
            long utime = Long.parseLong(info[14]);
            long stime = Long.parseLong(info[15]);
            long cutime = Long.parseLong(info[16]);
            long cstime = Long.parseLong(info[17]);
            long sum = utime + stime + cutime + cstime;
            MatrixLog.i(TAG, "sum:" + sum + " utime:" + utime + " stime:" + stime + " cutime:" + cutime + " cstime:" + cstime);
            return sum;

        } catch (Exception e) {
            return -1L;
        }
    }

    public static long getJiffies() {
        return getUJiffies(Process.myTid());
    }

    public static long getJiffies(long tid) {
        String schedStat = String.format("/proc/%s/task/%s/schedstat", Process.myPid(), tid);
        try {
            String content = getStringFromFile(schedStat);
            if (null == content) {
                return -1L;
            }
            String[] info = content.replaceAll("\n", "").split(" ");
            return Long.parseLong(info[2]);

        } catch (Exception e) {
            return -2L;
        }
    }

    public static long getUJiffies() {
        return getUJiffies(Process.myTid());
    }

    public static long getUJiffies(int tid) {
        String stat = String.format("/proc/%s/task/%s/stat", Process.myPid(), tid);
        try {
            String content = getStringFromFile(stat);
            if (null == content) {
                return -1L;
            }

            String[] info = content.replaceAll("\n", "").split(" ");
            return Long.parseLong(info[14]);

        } catch (Exception e) {
            return -1L;
        }
    }

    public static String convertStreamToString(InputStream is) throws IOException {
        BufferedReader reader = null;
        StringBuilder sb = new StringBuilder();
        try {
            reader = new BufferedReader(new InputStreamReader(is, "UTF-8"));
            String line = null;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
        } finally {
            if (null != reader) {
                reader.close();
            }
        }

        return sb.toString();
    }

    public static String getStringFromFile(String filePath) throws IOException {
        File fl = new File(filePath);
        FileInputStream fin = null;
        String ret;
        try {
            fin = new FileInputStream(fl);
            ret = convertStreamToString(fin);
        } finally {
            if (null != fin) {
                fin.close();
            }
        }
        return ret;
    }


}
