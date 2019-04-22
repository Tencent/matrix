package com.tencent.matrix.threadcanary;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;
import android.os.SystemClock;

import com.tencent.matrix.AppForegroundDelegate;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class ThreadWatcher extends Plugin {

    private static final String TAG = "Matrix.ThreadWatcher";
    private final ThreadConfig threadConfig;
    private DumpThreadJiffiesTask checkThreadTask;
    private List<List<ThreadGroupInfo>> pendingReport = new LinkedList<>();
    private long checkTime;
    private long checkBgTime;
    private long reportTime;
    private int limitCount;
    private Handler handler;

    public ThreadWatcher(ThreadConfig config) {
        this.threadConfig = config;
        this.checkTime = config.getCheckTime();
        this.reportTime = config.getReportTime();
        this.limitCount = config.getThreadLimitCount();
        this.checkBgTime = config.getCheckBgTime();
        this.checkThreadTask = new DumpThreadJiffiesTask();
        this.handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
    }

    public ThreadWatcher(ThreadConfig config, int limitCount) {
        this.threadConfig = config;
        this.checkTime = config.getCheckTime();
        this.reportTime = config.getReportTime();
        this.limitCount = limitCount;
        this.checkBgTime = config.getCheckBgTime();
        this.checkThreadTask = new DumpThreadJiffiesTask();
        this.handler = new Handler(MatrixHandlerThread.getDefaultHandlerThread().getLooper());
    }


    private final IThreadFilter mThreadFilter = new IThreadFilter() {
        @Override
        public boolean isFilter(ThreadInfo threadInfo) {
            if (null == threadConfig.getFilterThreadSet()) {
                return false;
            }
            for (String s : threadConfig.getFilterThreadSet()) {
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
        MatrixLog.i(TAG, "start!");
        MatrixHandlerThread.getDefaultMainHandler().post(new Runnable() {
            @Override
            public void run() {
                handler.post(checkThreadTask);
            }
        });
    }

    @Override
    public void stop() {
        super.stop();
        MatrixLog.i(TAG, "stop!");
        handler.removeCallbacks(checkThreadTask);
    }


    private class DumpThreadJiffiesTask implements Runnable {
        private final long mainTid = Process.myPid();

        private DumpThreadJiffiesTask() {
        }

        @Override
        public void run() {
            int processThreadCount = getProcessThreadCount();
            MatrixLog.i(TAG, "[DumpThreadJiffiesTask] run...[%s] limit:%s", processThreadCount, limitCount);
            if (limitCount >= processThreadCount) {
                return;
            }
            final Map<Long, ThreadInfo> appThreadsMap = getAppThreadsMap(mThreadFilter);
            List<ThreadInfo> linuxThreads = getThreadsInfo(new IThreadInfoIterator() {
                @Override
                public void next(ThreadInfo threadInfo) {
                    ThreadInfo appThreadInfo = appThreadsMap.get(threadInfo.tid);
                    threadInfo.name = threadInfo.name.replaceAll("-?[0-9]\\d*", "?");
                    if (null != appThreadInfo) {
                        if (threadInfo.tid == mainTid) {
                            threadInfo.name = "main";
                        } else {
                            appThreadInfo.name = threadInfo.name = appThreadInfo.name.replaceAll("-?[0-9]\\d*", "?");
                        }
                        threadInfo.stack = appThreadInfo.stack;
                    }
                }
            }, new IThreadFilter() {
                @Override
                public boolean isFilter(ThreadInfo threadInfo) {
                    return mThreadFilter.isFilter(threadInfo);
                }
            });

            HashMap<String, ThreadGroupInfo> threadGroupInfoMap = new HashMap<>();

            for (ThreadInfo threadInfo : linuxThreads) {
                ThreadGroupInfo groupInfo = threadGroupInfoMap.get(threadInfo.name);
                if (null == groupInfo) {
                    groupInfo = new ThreadGroupInfo(threadInfo.name);
                    threadGroupInfoMap.put(threadInfo.name, groupInfo);
                }
                groupInfo.list.add(threadInfo);
            }


            List<ThreadGroupInfo> threadGroupInfoList = new LinkedList<>(threadGroupInfoMap.values());

            Collections.sort(threadGroupInfoList, new Comparator<ThreadGroupInfo>() {
                @Override
                public int compare(ThreadGroupInfo o1, ThreadGroupInfo o2) {
                    return Long.compare(o2.getSize(), o1.getSize());
                }
            });

            long time = SystemClock.uptimeMillis();
            if (isForeground() && time - lastReportTime > reportTime) {
                for (List<ThreadGroupInfo> list : pendingReport) {
                    report(list);
                }
                lastReportTime = time;
                pendingReport.clear();
            } else {
                if (pendingReport.size() >= 8) {
                    pendingReport.remove(0);
                }
                pendingReport.add(threadGroupInfoList);
            }
            handler.postDelayed(this, AppForegroundDelegate.INSTANCE.isAppForeground() ? checkTime : checkBgTime);
        }
    }

    @Override
    public void onForeground(boolean isForeground) {
        super.onForeground(isForeground);
        handler.removeCallbacksAndMessages(null);
        if (null != checkThreadTask) {
            if (isForeground) {
                handler.postDelayed(checkThreadTask, checkTime);
            } else {
                handler.postDelayed(checkThreadTask, checkBgTime);
            }
        }
    }

    private long lastReportTime = SystemClock.uptimeMillis();

    private void report(List<ThreadGroupInfo> list) {
        if (list.isEmpty()) {
            return;
        }
        Issue issue = new Issue();
        issue.setTag(Constants.TAG_PLUGIN);
        JSONObject jsonObject = new JSONObject();
        issue.setContent(jsonObject);
        JSONArray listObj = new JSONArray();
        int threadCount = 0;
        try {
            jsonObject.put(Constants.REPORT_KEY_THREAD_GROUP_COUNT, list.size());
            jsonObject.put(Constants.REPORT_KEY_THREAD_GROUP_LIST, listObj);
            for (ThreadGroupInfo group : list) {
                JSONObject groupObj = new JSONObject();
                listObj.put(groupObj);
                groupObj.put(Constants.REPORT_KEY_THREAD_GROUP_NAME, group.name);
                groupObj.put(Constants.REPORT_KEY_THREAD_GROUP_THREAD_COUNT, group.getSize());
                JSONArray threadListObj = new JSONArray();
                groupObj.put(Constants.REPORT_KEY_THREAD_GROUP_THREAD_LIST, threadListObj);
                for (ThreadInfo threadInfo : group.list) {
                    threadCount++;
                    JSONObject threadObj = new JSONObject();
                    threadListObj.put(threadObj);
                    threadObj.put(Constants.REPORT_KEY_THREAD_INFO_TID, threadInfo.tid);
                    threadObj.put(Constants.REPORT_KEY_THREAD_INFO_STATE, threadInfo.state);
                    threadObj.put(Constants.REPORT_KEY_THREAD_INFO_JIFFIES, threadInfo.jiffies);
                    threadObj.put(Constants.REPORT_KEY_THREAD_INFO_STACK, threadInfo.stack);
                    threadObj.put(Constants.REPORT_KEY_THREAD_INFO_STACK_MD5, threadInfo.stackMD5);
                }
            }
            jsonObject.put(Constants.REPORT_KEY_THREAD_COUNT, threadCount);
        } catch (JSONException e) {
            MatrixLog.e(TAG, "%s %s", e, stackTraceToString(e.getStackTrace()));
        }
        onDetectIssue(issue);
    }

    public interface IThreadFilter {
        boolean isFilter(ThreadInfo threadInfo);
    }

    public static int getProcessThreadCount() {
        String status = String.format("/proc/%s/status", Process.myPid());
        try {
            String content = getStringFromFile(status).trim();
            String[] args = content.split("\n");
            for (String str : args) {
                if (str.startsWith("Threads")) {
                    Pattern p = Pattern.compile("\\d+");
                    Matcher matcher = p.matcher(str);
                    if (matcher.find()) {
                        return Integer.parseInt(matcher.group());
                    }
                }
            }
            MatrixLog.w(TAG, "[getProcessThreadCount] Wrong!", args[24]);
            return Integer.parseInt(args[24].trim());
        } catch (Exception e) {
            return 0;
        }
    }

    private static HashMap<Long, ThreadInfo> getAppThreadsMap(IThreadFilter filter) {
        HashSet<String> stackMD5Set = new HashSet<>();
        HashMap<Long, ThreadInfo> map = new HashMap<>();
        Map<Thread, StackTraceElement[]> stacks = Thread.getAllStackTraces();
        Set<Thread> set = stacks.keySet();
        for (Thread thread : set) {
            ThreadInfo threadInfo = new ThreadInfo();
            threadInfo.name = thread.getName();
            if (null != filter && filter.isFilter(threadInfo)) {
                continue;
            }
            if (thread.getState() == Thread.State.RUNNABLE) {
                threadInfo.stack = stackTraceToString(thread.getStackTrace());
                threadInfo.stackMD5 = MatrixUtil.getMD5String(threadInfo.stack);
            }
            if (stackMD5Set.contains(threadInfo.stackMD5)) {
                threadInfo.stack = "";
            } else {
                stackMD5Set.add(threadInfo.stackMD5);
            }
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


    private interface IThreadInfoIterator {
        void next(ThreadInfo info);
    }

    public static List<ThreadInfo> getThreadsInfo(IThreadInfoIterator iterator, IThreadFilter filter) {
        List<ThreadInfo> set = new LinkedList<>();
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
                        if (!filter.isFilter(threadInfo)) {
                            set.add(threadInfo);
                            iterator.next(threadInfo);
                        }
                    }
                } catch (Exception e) {
                    MatrixLog.e(TAG, "%s %s", e, stackTraceToString(e.getStackTrace()));
                }
            }
        }
        return set;
    }

    public static String stackTraceToString(final StackTraceElement[] stackTrace) {

        if ((stackTrace == null)) {
            return "";
        }

        StringBuilder t = new StringBuilder();
        for (int i = 2; i < stackTrace.length; i++) {
            t.append('[');
            t.append(stackTrace[i].getClassName());
            t.append(':');
            t.append(stackTrace[i].getMethodName());
            t.append("(" + stackTrace[i].getLineNumber() + ")]");
            t.append("\n");
        }
        return t.toString();
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

    private static class ThreadInfo {
        String name;
        long tid;
        boolean isHandlerThread;
        String state;
        long jiffies;
        String stack;
        String stackMD5;

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof ThreadInfo) {
                ThreadInfo info = (ThreadInfo) obj;
                return tid == info.tid;
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
            return String.format("%s %s %s %s %s", name, tid, state, jiffies, isHandlerThread);
        }
    }

    private static class ThreadGroupInfo {
        String name;
        List<ThreadInfo> list = new LinkedList<>();


        ThreadGroupInfo(String name) {
            this.name = name;
        }

        public int getSize() {
            return list.size();
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof ThreadInfo) {
                ThreadInfo info = (ThreadInfo) obj;
                return name.equals(info.name);
            } else {
                return false;
            }
        }

        @Override
        public int hashCode() {
            return name.hashCode();
        }

        @Override
        public String toString() {
            return name + " " + list + " " + getSize();
        }
    }

}
