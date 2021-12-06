package com.tencent.matrix.openglleak.statistics.resource;

import java.util.ArrayList;
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

    public List<OpenGLInfo> getCopyList() {
        List<OpenGLInfo> ll = new ArrayList<>();

        synchronized (mList) {
            for (OpenGLInfo item : mList) {
                if (item == null) {
                    break;
                }

                OpenGLInfo clone = new OpenGLInfo(item);
                ll.add(clone);
            }
        }

        return ll;
    }

    public void clear() {
        synchronized (mList) {
            mList.clear();
        }
    }
}
