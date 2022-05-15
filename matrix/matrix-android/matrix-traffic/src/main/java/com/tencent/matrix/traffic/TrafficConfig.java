package com.tencent.matrix.traffic;

import java.util.ArrayList;
import java.util.List;

public class TrafficConfig {
    public static final int STACK_TRACE_FILTER_MODE_FULL = 0;
    public static final int STACK_TRACE_FILTER_MODE_STARTS_WITH = 1;
    public static final int STACK_TRACE_FILTER_MODE_PATTERN = 2;
    private boolean rxCollectorEnable;
    private boolean txCollectorEnable;
    private boolean dumpStackTraceEnable;
    private boolean dumpNativeBackTraceEnable;
    //TODO
    //private boolean lookupIpAddressEnable;
    private int stackTraceFilterMode = 0;
    private String stackTraceFilterCore;
    private List<String> ignoreSoList = new ArrayList<>();

    public  TrafficConfig() {

    }

    public TrafficConfig(boolean rxCollectorEnable, boolean txCollectorEnable, boolean dumpStackTraceEnable) {
        this.rxCollectorEnable = rxCollectorEnable;
        this.txCollectorEnable = txCollectorEnable;
        this.dumpStackTraceEnable = dumpStackTraceEnable;
        this.dumpNativeBackTraceEnable = false;
    }

    public TrafficConfig(boolean rxCollectorEnable, boolean txCollectorEnable, boolean dumpStackTraceEnable, boolean dumpNativeBackTraceEnable) {
        this.rxCollectorEnable = rxCollectorEnable;
        this.txCollectorEnable = txCollectorEnable;
        this.dumpStackTraceEnable = dumpStackTraceEnable;
        this.dumpNativeBackTraceEnable = dumpNativeBackTraceEnable;
    }

    public boolean isRxCollectorEnable() {
        return rxCollectorEnable;
    }
    public void setRxCollectorEnable(boolean rxCollectorEnable) {
        this.rxCollectorEnable = rxCollectorEnable;
    }

    public boolean isTxCollectorEnable() {
        return txCollectorEnable;
    }
    public void setTxCollectorEnable(boolean txCollectorEnable) {
        this.txCollectorEnable = txCollectorEnable;
    }

    public boolean willDumpStackTrace() {
        return dumpStackTraceEnable;
    }

    public void setDumpStackTraceEnable(boolean dumpStackTraceEnable) {
        this.dumpStackTraceEnable = dumpStackTraceEnable;
    }

    public boolean willDumpNativeBackTrace() {
        return dumpNativeBackTraceEnable;
    }

    public void setDumpNativeBackTrace(boolean dumpNativeBackTraceEnable) {
        this.dumpNativeBackTraceEnable = dumpNativeBackTraceEnable;
    }

    public void addIgnoreSoFile(String soName) {
        ignoreSoList.add(soName);
    }

    public String[] getIgnoreSoFiles() {
        return ignoreSoList.toArray(new String[ignoreSoList.size()]);
    }

    public void setStackTraceFilterMode(int mode, String filterCore) {
        this.stackTraceFilterMode = mode;
        this.stackTraceFilterCore = filterCore;
    }

    public int getStackTraceFilterMode() {
        return this.stackTraceFilterMode;
    }

    public String getStackTraceFilterCore() {
        return this.stackTraceFilterCore;
    }
}
