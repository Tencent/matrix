package com.tencent.matrix.openglleak.statistics.source;

import android.os.Handler;

import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.util.LinkedList;
import java.util.List;

public class ResRecorderForCustomize implements ResRecordManager.Callback {

    private Handler mH;

    public ResRecorderForCustomize() {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
        ResRecordManager.getInstance().registerCallback(this);
    }

    private List<OpenGLInfo> mList = new LinkedList<>();

    @Override
    public void gen(final OpenGLInfo res) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                mList.add(res);
            }
        });
    }

    @Override
    public void delete(final OpenGLInfo res) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                mList.remove(res);
            }
        });
    }
}
