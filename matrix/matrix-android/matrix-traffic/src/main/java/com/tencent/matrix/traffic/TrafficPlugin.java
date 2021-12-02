package com.tencent.matrix.traffic;

import com.tencent.matrix.plugin.Plugin;

import java.util.HashMap;

public class TrafficPlugin extends Plugin {

    final private TrafficConfig trafficConfig;
    public static final int TYPE_GET_TRAFFIC_RX = 0;
    public static final int TYPE_GET_TRAFFIC_TX = 1;

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
        nativeInitMatrixTraffic(trafficConfig.isRxCollectorEnable(), trafficConfig.isTxCollectorEnable());
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
    public void clearTrafficInfo() {
        nativeClearTrafficInfo();
    }

    private static native void nativeInitMatrixTraffic(boolean rxEnable, boolean txEnable);
    private static native String nativeGetTrafficInfo();
    private static native String nativeGetAllStackTraceTrafficInfo();
    private static native void nativeReleaseMatrixTraffic();
    private static native void nativeEnableDumpJavaStackTrace(boolean enable);
    private static native void nativeClearTrafficInfo();
    private static native HashMap<String, String> nativeGetTrafficInfoMap(int type);

    public static String getJavaStackTrace() {
        StringBuilder stackTrace = new StringBuilder();
        for (StackTraceElement stackTraceElement : Thread.currentThread().getStackTrace()) {
            stackTrace.append(stackTraceElement.toString()).append("\n");
        }
        return stackTrace.toString();
    }
}
