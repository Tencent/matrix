/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.tencent.matrix.trace.tracer;

import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.os.Build;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.util.AppForegroundUtil;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class SignalAnrTracer extends Tracer {
    private static final String TAG = "SignalAnrTracer";

    private static final String CHECK_ANR_STATE_THREAD_NAME = "Check-ANR-State-Thread";
    private static final String ANR_DUMP_THREAD_NAME = "ANR-Dump-Thread";
    private static final int CHECK_ERROR_STATE_INTERVAL = 500;
    private static final int ANR_DUMP_MAX_TIME = 20000;
    private static long anrReportTimeout = ANR_DUMP_MAX_TIME;
    private static final int CHECK_ERROR_STATE_COUNT =
            ANR_DUMP_MAX_TIME / CHECK_ERROR_STATE_INTERVAL;
    private static final long FOREGROUND_MSG_THRESHOLD = -2000;
    private static final long BACKGROUND_MSG_THRESHOLD = -10000;
    private static boolean currentForeground = false;
    private static String sAnrTraceFilePath = "";
    private static String sPrintTraceFilePath = "";
    private static SignalAnrDetectedListener sSignalAnrDetectedListener;
    private static Application sApplication;
    private static boolean hasInit = false;
    public static boolean hasInstance = false;
    private static long anrMessageWhen = 0L;
    private static String anrMessageString = "";
    private static String cgroup = "";
    private static String stackTrace = "";
    private static String nativeBacktraceStackTrace = "";
    private static long lastReportedTimeStamp = 0;
    private static long onAnrDumpedTimeStamp = 0;

    static {
        System.loadLibrary("trace-canary");
    }

    private static class SimpleDeadLockDetector {
        static class ThreadNode {
            int threadId;
            String info;
            List<Integer> waitingList;
            int visit;  // 0 not visited, 1 visiting, 2 visited

            ThreadNode(int a, String b, List<Integer> c) {
                threadId = a;
                info = b;
                waitingList = c;
            }
        }

        private final Pattern threadPattern = Pattern.compile("^\"(.*?)\" .*? tid=(\\d+) \\w+$");
        private final Pattern lockHeldPattern = Pattern.compile("^  - .*? held by thread (\\d+)$");
        private int currentThreadId;
        private final StringBuilder currentSb = new StringBuilder();
        private final HashMap<Integer, ThreadNode> threadsWaitingForHeldLock = new HashMap<>();
        private List<Integer> waitingList;
        private String mainThreadInfo = "";
        private boolean threadInfoBegin = false;

        public void parseLine(String line) {

            if (line.isEmpty()) {
                // thread info end
                threadInfoBegin = false;

                if (currentSb.length() > 0 && waitingList != null && waitingList.size() > 0) {
                    String threadInfo = currentSb.toString();
                    if (currentThreadId == 1) {
                        // "currentThreadId" is a thin lock thread id. This is a small integer used by the
                        // thin lock implementation. This is not to be confused with the native thread's tid,
                        // nor is it the value returned by java.lang.Thread.getId --- this is a distinct value,
                        // used only for locking. usually, 0 is reserved to mean "invalid", 1 for main thread.
                        mainThreadInfo = threadInfo;
                    }

                    threadsWaitingForHeldLock.put(currentThreadId, new ThreadNode(currentThreadId, threadInfo, waitingList));
                    waitingList = null;
                }

            } else if (!threadInfoBegin) {
                Matcher m = threadPattern.matcher(line);
                if (m.find()) {
                    // new thread info begin
                    threadInfoBegin = true;

                    currentSb.setLength(0);
                    currentSb.append(line).append('\n');
                    waitingList = waitingList == null ? new ArrayList<Integer>(4) : waitingList;
                    try {
                        currentThreadId = Integer.parseInt(m.group(2));
                    } catch (Exception e) {
                        MatrixLog.e(TAG, e.toString());
                    }
                }
            } else {
                Matcher m = lockHeldPattern.matcher(line);
                if (m.find()) {
                    try {
                        int peerId = Integer.parseInt(m.group(1));
                        waitingList.add(peerId);
                    } catch (Exception e) {
                        MatrixLog.e(TAG, e.toString());
                    }
                }
                currentSb.append(line).append('\n');
            }
        }

        public boolean hasDeadLock() {
            parseLine("");  // ensure thread info parse complete
            return checkDeadLock();
        }

        @NonNull
        public String getMainThreadInfo() {
            return mainThreadInfo;
        }

        @NonNull
        public String getLockHeldThread1Info() {
            if (waitingList == null || waitingList.size() == 0) {
                return "";
            }
            int threadId = waitingList.get(0);
            ThreadNode node = threadsWaitingForHeldLock.get(threadId);
            return node == null ? "" : node.info;
        }

        @NonNull
        public String getLockHeldThread2Info() {
            if (waitingList == null || waitingList.size() == 0) {
                return "";
            }
            int threadId = waitingList.get(waitingList.size() - 1);
            ThreadNode node = threadsWaitingForHeldLock.get(threadId);
            return node == null ? "" : node.info;
        }

        @NonNull
        public List<Integer> getWaitingList() {
            return waitingList == null ? new ArrayList<Integer>() : waitingList;
        }

        private boolean checkDeadLock() {
            LinkedList<Integer> path = new LinkedList<>();
            for (Map.Entry<Integer, ThreadNode> nodeEntry : threadsWaitingForHeldLock.entrySet()) {
                if (nodeEntry.getValue().visit == 0) {
                    int ret;
                    if ((ret = dfsSearch(path, nodeEntry.getValue())) != -1) {
                        // retrieve cycle from path and save it in waitingList
                        while (path.size() > 0 && path.getFirst() != ret) {
                            path.removeFirst();
                        }
                        waitingList = path;
                        return true;
                    }
                }
            }

            // no cycle
            if (waitingList != null) {
                waitingList.clear();
            }
            return false;
        }

        // return the entry point if a cycle is found, else return -1.
        private int dfsSearch(LinkedList<Integer> path, ThreadNode node) {
            path.addLast(node.threadId);
            node.visit = 1;

            for (Integer peer : node.waitingList) {
                ThreadNode peerNode = threadsWaitingForHeldLock.get(peer);
                if (peerNode != null) {
                    if (peerNode.visit == 1) {
                        return peerNode.threadId;
                    }

                    int ret;
                    if (peerNode.visit == 0 && (ret = dfsSearch(path, peerNode)) != -1) {
                        return ret;
                    }
                }
            }

            node.visit = 2;
            path.removeLast();
            return -1;
        }
    }

    @Override
    protected void onAlive() {
        super.onAlive();
        if (!hasInit) {
            nativeInitSignalAnrDetective(sAnrTraceFilePath, sPrintTraceFilePath);
            AppForegroundUtil.INSTANCE.init();
            hasInit = true;
        }

    }

    @Override
    protected void onDead() {
        super.onDead();
        nativeFreeSignalAnrDetective();
    }

    public SignalAnrTracer(TraceConfig traceConfig) {
        hasInstance = true;
        sAnrTraceFilePath = traceConfig.anrTraceFilePath;
        sPrintTraceFilePath = traceConfig.printTraceFilePath;
    }

    public SignalAnrTracer(Application application) {
        hasInstance = true;
        sApplication = application;
    }

    public SignalAnrTracer(Application application, String anrTraceFilePath, String printTraceFilePath) {
        hasInstance = true;
        sAnrTraceFilePath = anrTraceFilePath;
        sPrintTraceFilePath = printTraceFilePath;
        sApplication = application;
    }

    public static void setAnrReportTimeout(long timeout) {
        anrReportTimeout = timeout;
    }

    public void setSignalAnrDetectedListener(SignalAnrDetectedListener listener) {
        sSignalAnrDetectedListener = listener;
    }

    public static String readCgroup() {
        StringBuilder ret = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream("/proc/self/cgroup")))) {
            String line;
            while ((line = reader.readLine()) != null) {
                ret.append(line).append("\n");
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }
        return ret.toString();
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    private static void confirmRealAnr(final boolean isSigQuit) {
        MatrixLog.i(TAG, "confirmRealAnr, isSigQuit = " + isSigQuit);
        boolean needReport = isMainThreadBlocked();
        if (needReport) {
            report(false, isSigQuit);
        } else {
            new Thread(new Runnable() {
                @Override
                public void run() {
                    checkErrorStateCycle(isSigQuit);
                }
            }, CHECK_ANR_STATE_THREAD_NAME).start();
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    @Keep
    private synchronized static void onANRDumped() {
        final CountDownLatch anrDumpLatch = new CountDownLatch(1);
        new Thread(new Runnable() {
            @Override
            public void run() {
                onAnrDumpedTimeStamp = System.currentTimeMillis();
                MatrixLog.i(TAG, "onANRDumped");
                stackTrace = Utils.getMainThreadJavaStackTrace();
                MatrixLog.i(TAG, "onANRDumped, stackTrace = %s, duration = %d", stackTrace, (System.currentTimeMillis() - onAnrDumpedTimeStamp));
                cgroup = readCgroup();
                MatrixLog.i(TAG, "onANRDumped, read cgroup duration = %d", (System.currentTimeMillis() - onAnrDumpedTimeStamp));
                currentForeground = AppForegroundUtil.isInterestingToUser();
                MatrixLog.i(TAG, "onANRDumped, isInterestingToUser duration = %d", (System.currentTimeMillis() - onAnrDumpedTimeStamp));
                confirmRealAnr(true);
                anrDumpLatch.countDown();
            }
        }, ANR_DUMP_THREAD_NAME).start();

        try {
            anrDumpLatch.await(anrReportTimeout, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            //empty here
        }
    }

    @Keep
    private static void onANRDumpTrace() {
        try {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(new File(sAnrTraceFilePath)), "UTF-8"))) {
                String line;
                SimpleDeadLockDetector detector = new SimpleDeadLockDetector();
                while ((line = reader.readLine()) != null) {
                    detector.parseLine(line);
                    MatrixLog.i(TAG, line);
                }
                if (sSignalAnrDetectedListener != null) {
                    if (detector.hasDeadLock()) {
                        sSignalAnrDetectedListener.onDeadLockAnrDetected(
                                detector.getMainThreadInfo(), detector.getLockHeldThread1Info(),
                                detector.getLockHeldThread2Info(), detector.getWaitingList());
                    } else if (detector.getMainThreadInfo().contains("android.os.MessageQueue.nativePollOnce")) {
                        sSignalAnrDetectedListener.onMainThreadStuckAtNativePollOnce(detector.getMainThreadInfo());
                    }
                }
            } catch (Throwable t) {
                MatrixLog.e(TAG, "printFileByLine failed e : " + t.getMessage());
            }
        } catch (Throwable t) {
            MatrixLog.e(TAG, "onANRDumpTrace error: %s", t.getMessage());
        }
    }

    @Keep
    private static void onPrintTrace() {
        try {
            MatrixUtil.printFileByLine(TAG, sPrintTraceFilePath);
        } catch (Throwable t) {
            MatrixLog.e(TAG, "onPrintTrace error: %s", t.getMessage());
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    @Keep
    private static void onNativeBacktraceDumped() {
        MatrixLog.i(TAG, "happens onNativeBacktraceDumped");
        if (System.currentTimeMillis() - lastReportedTimeStamp < ANR_DUMP_MAX_TIME) {
            MatrixLog.i(TAG, "report SIGQUIT recently, just return");
            return;
        }
        nativeBacktraceStackTrace = Utils.getMainThreadJavaStackTrace();
        MatrixLog.i(TAG, "happens onNativeBacktraceDumped, mainThreadStackTrace = " + stackTrace);

        confirmRealAnr(false);
    }

    private static void report(boolean fromProcessErrorState, boolean isSigQuit) {
        try {
            if (sSignalAnrDetectedListener != null) {
                if (isSigQuit) {
                    sSignalAnrDetectedListener.onAnrDetected(stackTrace, anrMessageString, anrMessageWhen, fromProcessErrorState, cgroup);
                } else {
                    sSignalAnrDetectedListener.onNativeBacktraceDetected(nativeBacktraceStackTrace, anrMessageString, anrMessageWhen, fromProcessErrorState);
                }
                return;
            }

            TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
            if (null == plugin) {
                return;
            }

            String scene = AppActiveMatrixDelegate.INSTANCE.getVisibleScene();

            JSONObject jsonObject = new JSONObject();
            jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
            if (isSigQuit) {
                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.SIGNAL_ANR);
                jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, stackTrace);
            } else {
                jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.SIGNAL_ANR_NATIVE_BACKTRACE);
                jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, nativeBacktraceStackTrace);
            }
            jsonObject.put(SharePluginInfo.ISSUE_SCENE, scene);
            jsonObject.put(SharePluginInfo.ISSUE_PROCESS_FOREGROUND, currentForeground);

            Issue issue = new Issue();
            issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
            issue.setContent(jsonObject);
            plugin.onDetectIssue(issue);
            MatrixLog.e(TAG, "happens real ANR : %s ", jsonObject.toString());

        } catch (JSONException e) {
            MatrixLog.e(TAG, "[JSONException error: %s", e);
        } finally {
            lastReportedTimeStamp = System.currentTimeMillis();
        }
    }


    @RequiresApi(api = Build.VERSION_CODES.M)
    private static boolean isMainThreadBlocked() {
        try {
            MessageQueue mainQueue = Looper.getMainLooper().getQueue();
            Field field = mainQueue.getClass().getDeclaredField("mMessages");
            field.setAccessible(true);
            final Message mMessage = (Message) field.get(mainQueue);
            if (mMessage != null) {
                anrMessageString = mMessage.toString();
                MatrixLog.i(TAG, "anrMessageString = " + anrMessageString);
                long when = mMessage.getWhen();
                if (when == 0) {
                    return false;
                }
                long time = when - SystemClock.uptimeMillis();
                anrMessageWhen = time;
                long timeThreshold = BACKGROUND_MSG_THRESHOLD;
                if (currentForeground) {
                    timeThreshold = FOREGROUND_MSG_THRESHOLD;
                }
                return time < timeThreshold;
            } else {
                MatrixLog.i(TAG, "mMessage is null");
            }
        } catch (Exception e) {
            return false;
        }
        return false;
    }


    private static void checkErrorStateCycle(boolean isSigQuit) {
        int checkErrorStateCount = 0;
        while (checkErrorStateCount < CHECK_ERROR_STATE_COUNT) {
            try {
                checkErrorStateCount++;
                boolean myAnr = checkErrorState();
                if (myAnr) {
                    report(true, isSigQuit);
                    break;
                }

                Thread.sleep(CHECK_ERROR_STATE_INTERVAL);
            } catch (Throwable t) {
                MatrixLog.e(TAG, "checkErrorStateCycle error, e : " + t.getMessage());
                break;
            }
        }
    }

    private static boolean checkErrorState() {
        try {
            MatrixLog.i(TAG, "[checkErrorState] start");
            Application application =
                    sApplication == null ? Matrix.with().getApplication() : sApplication;
            ActivityManager am = (ActivityManager) application
                    .getSystemService(Context.ACTIVITY_SERVICE);

            List<ActivityManager.ProcessErrorStateInfo> procs = am.getProcessesInErrorState();
            if (procs == null) {
                MatrixLog.i(TAG, "[checkErrorState] procs == null");
                return false;
            }

            for (ActivityManager.ProcessErrorStateInfo proc : procs) {
                MatrixLog.i(TAG, "[checkErrorState] found Error State proccessName = %s, proc.condition = %d", proc.processName, proc.condition);

                if (proc.uid != android.os.Process.myUid()
                        && proc.condition == ActivityManager.ProcessErrorStateInfo.NOT_RESPONDING) {
                    MatrixLog.i(TAG, "maybe received other apps ANR signal");
                    return false;
                }

                if (proc.pid != android.os.Process.myPid()) continue;

                if (proc.condition != ActivityManager.ProcessErrorStateInfo.NOT_RESPONDING) {
                    continue;
                }

                MatrixLog.i(TAG, "error sate longMsg = %s", proc.longMsg);

                return true;
            }
            return false;
        } catch (Throwable t) {
            MatrixLog.e(TAG, "[checkErrorState] error : %s", t.getMessage());
        }
        return false;
    }

    public static void printTrace() {
        if (!hasInstance) {
            MatrixLog.e(TAG, "SignalAnrTracer has not been initialize");
            return;
        }
        if (sPrintTraceFilePath.equals("")) {
            MatrixLog.e(TAG, "PrintTraceFilePath has not been set");
            return;
        }
        nativePrintTrace();
    }

    private static native void nativeInitSignalAnrDetective(String anrPrintTraceFilePath, String printTraceFilePath);

    private static native void nativeFreeSignalAnrDetective();

    private static native void nativePrintTrace();

    public interface SignalAnrDetectedListener {
        void onAnrDetected(String stackTrace, String mMessageString, long mMessageWhen, boolean fromProcessErrorState, String cpuset);

        void onNativeBacktraceDetected(String backtrace, String mMessageString, long mMessageWhen, boolean fromProcessErrorState);

        void onDeadLockAnrDetected(String mainThreadStackTrace, String lockHeldThread1, String lockHeldThread2, List<Integer> waitingList);

        void onMainThreadStuckAtNativePollOnce(String mainThreadStackTrace);
    }
}
