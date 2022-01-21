package com.tencent.matrix.trace.tracer;

import static com.tencent.matrix.trace.constants.Constants.DEFAULT_TOUCH_EVENT_LAG;

import androidx.annotation.Keep;

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
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONObject;

public class TouchEventLagTracer extends Tracer {
    private static final String TAG = "Matrix.TouchEventLagTracer";
    private static TraceConfig traceConfig;
    private static long lastLagTime = 0;
    private static String currentLagFdStackTrace;

    static {
        System.loadLibrary("trace-canary");
    }

    public TouchEventLagTracer(TraceConfig config) {
        traceConfig = config;
    }

    @Override
    public synchronized void onAlive() {
        super.onAlive();
        if (traceConfig.isTouchEventTraceEnable()) {
            nativeInitTouchEventLagDetective(traceConfig.touchEventLagThreshold);
        }
    }

    @Override
    public void onDead() {
        super.onDead();

    }

    public static native void nativeInitTouchEventLagDetective(int lagThreshold);

    @Keep
    private static void onTouchEventLagDumpTrace(int fd) {
        MatrixLog.e(TAG, "onTouchEventLagDumpTrace, fd = " + fd);
        currentLagFdStackTrace = Utils.getMainThreadJavaStackTrace();
    }
    @Keep
    private static void onTouchEventLag(final int fd) {
        MatrixLog.e(TAG, "onTouchEventLag, fd = " + fd);
        MatrixHandlerThread.getDefaultHandler().post(new Runnable() {
            @Override
            public void run() {
                try {
                    if (System.currentTimeMillis() - lastLagTime < DEFAULT_TOUCH_EVENT_LAG * 2) {
                        return;
                    }
                    MatrixLog.i(TAG, "onTouchEventLag report");

                    lastLagTime = System.currentTimeMillis();

                    TracePlugin plugin = Matrix.with().getPluginByClass(TracePlugin.class);
                    if (null == plugin) {
                        return;
                    }

                    String stackTrace = currentLagFdStackTrace;
                    boolean currentForeground = AppForegroundUtil.isInterestingToUser();
                    String scene = AppActiveMatrixDelegate.INSTANCE.getVisibleScene();

                    JSONObject jsonObject = new JSONObject();
                    jsonObject = DeviceUtil.getDeviceInfo(jsonObject, Matrix.with().getApplication());
                    jsonObject.put(SharePluginInfo.ISSUE_STACK_TYPE, Constants.Type.LAG_TOUCH);
                    jsonObject.put(SharePluginInfo.ISSUE_SCENE, scene);
                    jsonObject.put(SharePluginInfo.ISSUE_THREAD_STACK, stackTrace);
                    jsonObject.put(SharePluginInfo.ISSUE_PROCESS_FOREGROUND, currentForeground);

                    Issue issue = new Issue();
                    issue.setTag(SharePluginInfo.TAG_PLUGIN_EVIL_METHOD);
                    issue.setContent(jsonObject);
                    plugin.onDetectIssue(issue);

                } catch (Throwable t) {
                    MatrixLog.e(TAG, "Matrix error, error = " + t.getMessage());
                }
            }
        });
    }
}
