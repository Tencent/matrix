package com.tencent.matrix.trace.tracer;

import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.os.Build;
import android.os.Environment;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;

import androidx.annotation.Keep;
import androidx.annotation.RequiresApi;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.core.AppMethodBeat;
import com.tencent.matrix.trace.util.AppForegroundUtil;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.util.List;

public class SignalAnrTracer extends Tracer{
    private static final String TAG = "SignalAnrTracer";

    private static final int CHECK_ERROR_STATE_INTERVAL = 500;
    private static final int ANR_DUMP_MAX_TIME = 20000;
    private static final int CHECK_ERROR_STATE_COUNT = ANR_DUMP_MAX_TIME / CHECK_ERROR_STATE_INTERVAL;
    private static final long FOREGROUND_MSG_THRESHOLD = -2000;
    private static final long BACKGROUND_MSG_THRESHOLD = -10000;
    private static boolean currentForeground = false;
    private static String sAnrTraceFilePath = "";
    private static String sPrintTraceFilePath = "";
    private static SignalAnrDetectedListener sSignalAnrDetectedListener;
    private static Application sApplication;
    private static boolean hasInit = false;
    public static boolean hasInstance = false;


    static {
        System.loadLibrary("trace-canary");
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

    public SignalAnrTracer(Application application, String  anrTraceFilePath, String printTraceFilePath) {
        hasInstance = true;
        sAnrTraceFilePath = anrTraceFilePath;
        sPrintTraceFilePath = printTraceFilePath;
        sApplication = application;
    }

    public void setSignalAnrDetectedListener(SignalAnrDetectedListener listener) {
        sSignalAnrDetectedListener = listener;
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    @Keep
    private static void onANRDumped(final int status, final String dumpFile) {
        currentForeground = AppForegroundUtil.isInterestingToUser();
        boolean needReport = firstCheckMessage();

        if (needReport) {
            report();
        } else {
            checkErrorStateCycle();
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    @Keep
    private static void onANRDumpTrace() {
        try {
            String trace = MatrixUtil.getStringFromFile(sAnrTraceFilePath);
            MatrixLog.i(TAG, trace);
        } catch (IOException e) {
            MatrixLog.e(TAG, "onANRDumpTrace error: %s", e.getMessage());
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    @Keep
    private static void onPrintTrace() {
        try {
            String trace = MatrixUtil.getStringFromFile(sPrintTraceFilePath);
            MatrixLog.i(TAG, trace);
        } catch (IOException e) {
            MatrixLog.e(TAG, "onANRDumpTrace error: %s", e.getMessage());
        }
    }

    private static void report() {
        try {
            String stackTrace = Utils.getMainThreadJavaStackTrace();
            if (sSignalAnrDetectedListener != null) {
                sSignalAnrDetectedListener.onAnrDetected(stackTrace);
                return;
            }


            TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
            if (null == plugin) {
                return;
            }

            String scene = AppMethodBeat.getVisibleScene();

            JSONObject jsonObject = new JSONObject();
            jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
            jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.SIGNAL_ANR);
            jsonObject.put(SharePluginInfo.ISSUE_SCENE, scene);
            jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, stackTrace);
            jsonObject.put(SharePluginInfo.ISSUE_PROCESS_FOREGROUND, currentForeground);

            Issue issue = new Issue();
            issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
            issue.setContent(jsonObject);
            plugin.onDetectIssue(issue);
            MatrixLog.e(TAG, "happens real ANR : %s ", jsonObject.toString());

        } catch (JSONException e) {
            MatrixLog.e(TAG, "[JSONException error: %s", e);
        }
    }


    private static boolean firstCheckMessage(){
        try {
            MessageQueue mainQueue = Looper.getMainLooper().getQueue();
            Field field = mainQueue.getClass().getDeclaredField("mMessages");
            field.setAccessible(true);
            final Message mMessage = (Message) field.get(mainQueue);
            if (mMessage != null) {
                long when = mMessage.getWhen();
                if(when == 0) {
                    return false;
                }
                long time = when - SystemClock.uptimeMillis();
                long timeThreshold = BACKGROUND_MSG_THRESHOLD;
                if (currentForeground) {
                    timeThreshold = FOREGROUND_MSG_THRESHOLD;
                }
                return time < timeThreshold;
            }
        } catch (Exception e){
            return false;
        }
        return false;
    }


    private static void checkErrorStateCycle(){
        int checkErrorStateCount = 0;
        while(checkErrorStateCount < CHECK_ERROR_STATE_COUNT) {
            checkErrorStateCount++;
            boolean myAnr = checkErrorState();
            if(myAnr){
                break;
            }
        }
    }

    private static boolean checkErrorState() {
        try {
            Application application = sApplication == null ? Matrix.with().getApplication() : sApplication;
            ActivityManager am = (ActivityManager) application
                    .getSystemService(Context.ACTIVITY_SERVICE);

            List<ActivityManager.ProcessErrorStateInfo> procs = am.getProcessesInErrorState();
            if (procs == null) return false;

            for (ActivityManager.ProcessErrorStateInfo proc : procs) {
                MatrixLog.i(TAG, "[checkErrorState] found Error State proccessName = %s, proc.condition = %d", proc.processName, proc.condition);

                if(proc.uid != android.os.Process.myUid() && proc.condition == ActivityManager.ProcessErrorStateInfo.NOT_RESPONDING){
                    MatrixLog.i(TAG, "maybe received other apps ANR signal");
                }

                if (proc.pid != android.os.Process.myPid()) continue;

                if (proc.condition != ActivityManager.ProcessErrorStateInfo.NOT_RESPONDING) continue;

                return true;
            }
            return false;
        } catch (Throwable t){
            MatrixLog.e(TAG,"[checkErrorState] error : %s", t.getMessage());
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
        void onAnrDetected(String stackTrace);
    }
}
