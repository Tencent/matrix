package com.tencent.matrix.openglleak.statistics.resource;

import java.util.LinkedList;
import java.util.List;

public class ResRecorder implements ResRecordManager.Callback {

    public ResRecorder() {
    }

    private final List<OpenGLInfo> mList = new LinkedList<>();

    public void start() {
        ResRecordManager.getInstance().registerCallback(this);
    }

    public void end() {
        ResRecordManager.getInstance().unregisterCallback(this);
    }

    @Override
    public void gen(final OpenGLInfo res) {
        synchronized (mList) {
            mList.add(res);
        }
    }

    @Override
    public void delete(final OpenGLInfo res) {
        synchronized (mList) {
            mList.remove(res);
        }
    }

    public List<OpenGLInfo> getCurList() {
        return mList;
    }

    public void clear() {
        synchronized (mList) {
            mList.clear();
        }
    }
}
