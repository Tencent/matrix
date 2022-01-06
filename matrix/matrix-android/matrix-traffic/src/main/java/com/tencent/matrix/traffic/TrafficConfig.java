package com.tencent.matrix.traffic;

import java.util.ArrayList;
import java.util.List;

public class TrafficConfig {
    private boolean rxCollectorEnable;
    private boolean txCollectorEnable;
    private boolean dumpStackTraceEnable;
    private boolean dumpNativeBackTraceEnable;
    private boolean lookupIpAddressEnable;
    private List<String> ignoreSoList = new ArrayList<>();

    public  TrafficConfig() {

    }
    public TrafficConfig(boolean rxCollectorEnable, boolean txCollectorEnable, boolean dumpStackTraceEnable, boolean dumpNativeBackTraceEnable, boolean lookupIpAddressEnable) {
        this.rxCollectorEnable = rxCollectorEnable;
        this.txCollectorEnable = txCollectorEnable;
        this.dumpStackTraceEnable = dumpStackTraceEnable;
        this.dumpNativeBackTraceEnable = dumpNativeBackTraceEnable;
        this.lookupIpAddressEnable = lookupIpAddressEnable;
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

    public boolean willLookupIpAddress() {
        return lookupIpAddressEnable;
    }

    public void setLookupIpAddressEnable(boolean lookupIpAddressEnable) {
        this.lookupIpAddressEnable = lookupIpAddressEnable;
    }

    public void addIgnoreSoFile(String soName) {
        ignoreSoList.add(soName);
    }

    public String[] getIgnoreSoFiles() {
        return ignoreSoList.toArray(new String[ignoreSoList.size()]);
    }
}
