package com.tencent.matrix.openglleak.statistics;

import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.resource.ResRecorder;

import java.util.List;

public class CustomizeLeakMonitor {

    private ResRecorder mResRecorder;

    public CustomizeLeakMonitor() {
        mResRecorder = new ResRecorder();
    }

    public void checkStart() {
        mResRecorder.clear();
        mResRecorder.start();
    }

    public List<OpenGLInfo> checkEnd() {
        mResRecorder.end();
        return mResRecorder.getCopyList();
    }

}
