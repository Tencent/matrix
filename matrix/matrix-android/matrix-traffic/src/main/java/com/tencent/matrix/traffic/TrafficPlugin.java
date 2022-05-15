package com.tencent.matrix.traffic;

import androidx.annotation.Keep;

import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class TrafficPlugin extends Plugin {

    private static final String TAG = "TrafficPlugin";
    final private TrafficConfig trafficConfig;
    public static final int TYPE_GET_TRAFFIC_RX = 0;
    public static final int TYPE_GET_TRAFFIC_TX = 1;

    private static int stackTraceFilterMode = 0;
    private static String stackTraceFilterCore = "";
    private static final Map<String, String> hashStackTraceMap = new ConcurrentHashMap<>();
    private static final Map<String, String> keyHashMap = new ConcurrentHashMap<>();

    //TODO
    //public static final int TYPE_GET_TRAFFIC_ALL = 2;

    static {
        System.loadLibrary("matrix-traffic");
    }

    public TrafficPlugin(TrafficConfig trafficConfig) {
        this.trafficConfig = trafficConfig;
    }

    @Override
    public void start() {
        if (isPluginStarted()) {
            return;
        }
        super.start();
        MatrixLog.i(TAG, "start");
        String[] ignoreSoFiles = trafficConfig.getIgnoreSoFiles();
        stackTraceFilterMode = trafficConfig.getStackTraceFilterMode();
        stackTraceFilterCore = trafficConfig.getStackTraceFilterCore();
        nativeInitMatrixTraffic(trafficConfig.isRxCollectorEnable(), trafficConfig.isTxCollectorEnable(), trafficConfig.willDumpStackTrace(), trafficConfig.willDumpNativeBackTrace(), ignoreSoFiles);
    }


    @Override
    public void stop() {
        if (isPluginStopped()) {
            return;
        }
        super.stop();
        nativeReleaseMatrixTraffic();
    }


    public HashMap<String, String> getTrafficInfoMap(int type) {
        return nativeGetTrafficInfoMap(type);
    }

    public String getStackTraceByMd5(String md5) {
        return hashStackTraceMap.get(md5);
    }

    public String getJavaStackTraceByKey(String key) {
        String md5 = keyHashMap.get(key);
        if (md5 == null || md5.isEmpty()) {
            return "";
        }
        return hashStackTraceMap.get(md5);
    }
    public String getNativeBackTraceByKey(String key) {
        return nativeGetNativeBackTraceByKey(key);
    }

    public void clearTrafficInfo() {
        keyHashMap.clear();
        hashStackTraceMap.clear();
        nativeClearTrafficInfo();
    }

    private static native void nativeInitMatrixTraffic(boolean rxEnable, boolean txEnable, boolean dumpStackTrace, boolean dumpNativeBackTrace, String[] ignoreSoFiles);
    private static native String nativeGetTrafficInfo();
    private static native String nativeGetAllStackTraceTrafficInfo();
    private static native void nativeReleaseMatrixTraffic();
    private static native void nativeClearTrafficInfo();
    private static native HashMap<String, String> nativeGetTrafficInfoMap(int type);
    private static native String nativeGetNativeBackTraceByKey(String key);

    @Keep
    private static void setFdStackTrace(String key) {
        StringBuilder stackTrace = new StringBuilder();
        StackTraceElement[] stackTraceElements = Thread.currentThread().getStackTrace();
        for (int line = 0; line < stackTraceElements.length; line++) {
            String stackTraceLine = stackTraceElements[line].toString();
            boolean willAppend = false;
            if (stackTraceFilterMode == TrafficConfig.STACK_TRACE_FILTER_MODE_FULL) {
                willAppend = true;
            } else if (stackTraceFilterMode == TrafficConfig.STACK_TRACE_FILTER_MODE_STARTS_WITH) {
                if (stackTraceLine.startsWith(stackTraceFilterCore)) {
                    willAppend = true;
                }
            } else if (stackTraceFilterMode == TrafficConfig.STACK_TRACE_FILTER_MODE_PATTERN) {
                if (stackTraceLine.matches(stackTraceFilterCore)) {
                    willAppend = true;
                }
            }
            if (willAppend) {
                stackTrace.append(stackTraceLine).append("\n");
            }

        }
        String md5 = MatrixUtil.getMD5String(stackTrace.toString());
        if (!hashStackTraceMap.containsKey(md5)) {
            hashStackTraceMap.put(md5, stackTrace.toString());
        }
        keyHashMap.put(key, md5);
    }
}
