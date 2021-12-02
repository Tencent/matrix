package com.tencent.matrix.traffic;

public class TrafficConfig {
    private boolean rxCollectorEnable;
    private boolean txCollectorEnable;

    public  TrafficConfig() {

    }
    public TrafficConfig(boolean rxCollectorEnable, boolean txCollectorEnable) {
        this.rxCollectorEnable = rxCollectorEnable;
        this.txCollectorEnable = txCollectorEnable;
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
}
