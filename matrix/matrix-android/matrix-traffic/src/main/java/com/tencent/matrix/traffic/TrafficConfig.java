package com.tencent.matrix.traffic;

public class TrafficConfig {
    private boolean rxCollectorEnable;
    private boolean txCollectorEnable;
    private boolean dumpStackTrace;

    public  TrafficConfig() {

    }
    public TrafficConfig(boolean rxCollectorEnable, boolean txCollectorEnable, boolean dumpStackTrace) {
        this.rxCollectorEnable = rxCollectorEnable;
        this.txCollectorEnable = txCollectorEnable;
        this.dumpStackTrace = dumpStackTrace;
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
        return dumpStackTrace;
    }

    public void setDumpStackTrace(boolean dumpStackTrace) {
        this.dumpStackTrace = dumpStackTrace;
    }
}
