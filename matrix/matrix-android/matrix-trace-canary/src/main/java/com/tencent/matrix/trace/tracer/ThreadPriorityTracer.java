package com.tencent.matrix.trace.tracer;

import androidx.annotation.Keep;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.SharePluginInfo;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONObject;

public class ThreadPriorityTracer extends Tracer{

    private static final String TAG = "ThreadPriorityTracer";
    private static MainThreadPriorityModifiedListener sMainThreadPriorityModifiedListener;

    static {
        System.loadLibrary("anr-canary");
    }

    @Override
    protected void onAlive() {
        super.onAlive();
        nativeInitMainThreadPriorityDetective();
    }

    @Override
    protected void onDead() {
        super.onDead();
    }

    public void setMainThreadPriorityModifiedListener(MainThreadPriorityModifiedListener mainThreadPriorityModifiedListener) {
        this.sMainThreadPriorityModifiedListener = mainThreadPriorityModifiedListener;
    }

    private static native void nativeInitMainThreadPriorityDetective();

    @Keep
    private static void onMainThreadPriorityModified(int priority) {
        try {

            if (sMainThreadPriorityModifiedListener != null) {
                sMainThreadPriorityModifiedListener.onMainThreadPriorityModified(priority);
                return;
            }

            TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
            if (null == plugin) {
                return;
            }

            String stackTrace = Utils.getMainThreadJavaStackTrace();

            JSONObject jsonObject = new JSONObject();
            jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
            jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.PRIORITY_MODIFIED);
            jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, stackTrace);
            jsonObject.put(SharePluginInfo.ISSUE_PROCESS_PRIORITY, priority);

            Issue issue = new Issue();
            issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
            issue.setContent(jsonObject);
            plugin.onDetectIssue(issue);
            MatrixLog.e(TAG, "happens MainThreadPriorityModified : %s ", jsonObject.toString());
        } catch (Throwable t) {
            MatrixLog.e(TAG, "onMainThreadPriorityModified error: %s", t.getMessage());
        }
    }

    @Keep
    private static void onMainThreadTimerSlackModified(long timerSlack) {
        try {

            if (sMainThreadPriorityModifiedListener != null) {
                sMainThreadPriorityModifiedListener.onMainThreadTimerSlackModified(timerSlack);
                return;
            }

            TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
            if (null == plugin) {
                return;
            }

            String stackTrace = Utils.getMainThreadJavaStackTrace();

            JSONObject jsonObject = new JSONObject();
            jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
            jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.TIMERSLACK_MODIFIED);
            jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, stackTrace);
            jsonObject.put(SharePluginInfo.ISSUE_PROCESS_TIMER_SLACK, timerSlack);

            Issue issue = new Issue();
            issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
            issue.setContent(jsonObject);
            plugin.onDetectIssue(issue);
            MatrixLog.e(TAG, "happens MainThreadPriorityModified : %s ", jsonObject.toString());
        } catch (Throwable t) {
            MatrixLog.e(TAG, "onMainThreadPriorityModified error: %s", t.getMessage());
        }

    }

    public interface MainThreadPriorityModifiedListener {
        void onMainThreadPriorityModified(int priority);
        void onMainThreadTimerSlackModified(long timerSlack);
    }
}
