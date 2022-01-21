package com.tencent.matrix.traffic;

import androidx.annotation.Keep;

import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.util.MatrixLog;

import java.util.HashMap;
import java.util.concurrent.ConcurrentHashMap;

public class TrafficPlugin extends Plugin {

    private static final String TAG = "TrafficPlugin";
    final private TrafficConfig trafficConfig;
    public static final int TYPE_GET_TRAFFIC_RX = 0;
    public static final int TYPE_GET_TRAFFIC_TX = 1;

    private static final ConcurrentHashMap<String, String> stackTraceMap = new ConcurrentHashMap<>();

    //TODO will be done in next upgrade
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
        nativeInitMatrixTraffic(trafficConfig.isRxCollectorEnable(), trafficConfig.isTxCollectorEnable(), trafficConfig.willDumpStackTrace(), trafficConfig.willDumpNativeBackTrace(), trafficConfig.willLookupIpAddress(), ignoreSoFiles);
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

    public ConcurrentHashMap<String, String> getStackTraceMap() {
        return stackTraceMap;
    }

    public void clearTrafficInfo() {
        stackTraceMap.clear();
        nativeClearTrafficInfo();
    }

    private static native void nativeInitMatrixTraffic(boolean rxEnable, boolean txEnable, boolean dumpStackTrace, boolean dumpNativeBackTrace, boolean lookupIpAddress, String[] ignoreSoFiles);
    private static native String nativeGetTrafficInfo();
    private static native String nativeGetAllStackTraceTrafficInfo();
    private static native void nativeReleaseMatrixTraffic();
    private static native void nativeClearTrafficInfo();
    private static native HashMap<String, String> nativeGetTrafficInfoMap(int type);

    @Keep
    private static void setStackTrace(String threadName, String nativeBackTrace) {
        MatrixLog.i(TAG, "setStackTrace, threadName = " + threadName, "nativeBackTrace = " + nativeBackTrace);
        StringBuilder stackTrace = new StringBuilder(nativeBackTrace);
        StackTraceElement[] stackTraceElements = Thread.currentThread().getStackTrace();
        for (int line = 3; line < stackTraceElements.length; line++) {
            stackTrace.append(stackTraceElements[line]).append("\n");
        }

        stackTraceMap.put(threadName, stackTrace.toString());
    }

    public void clearStackTrace() {
        stackTraceMap.clear();
    }
}
