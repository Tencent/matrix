package com.tencent.matrix.openglleak;

public interface OpenglReportCallback {

    void onExpProcessSuccess();

    void onExpProcessFail();

    void onHookSuccess();

    void onHookFail();

}
