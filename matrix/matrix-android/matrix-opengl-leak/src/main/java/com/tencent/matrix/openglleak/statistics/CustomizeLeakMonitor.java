package com.tencent.matrix.openglleak.statistics;

import com.tencent.matrix.openglleak.statistics.source.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.source.ResRecorderForCustomize;

import java.util.List;

public class CustomizeLeakMonitor {

    private ResRecorderForCustomize mResRecorder;

    public CustomizeLeakMonitor() {
        mResRecorder = new ResRecorderForCustomize();
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
