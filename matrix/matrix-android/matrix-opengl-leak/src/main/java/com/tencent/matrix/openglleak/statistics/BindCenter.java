package com.tencent.matrix.openglleak.statistics;

public class BindCenter {

    private static final String TAG = "matrix.BindCenter";

    private static final BindCenter mInstance = new BindCenter();

    public static BindCenter getInstance() {
        return mInstance;
    }

    private BindCenter() {
    }

    public void glBindResource(OpenGLInfo.TYPE type, int target, long eglContextId, OpenGLInfo info) {
        BindMap.getInstance().putBindInfo(type, eglContextId, target, info);
    }

    public OpenGLInfo getCurrentResourceIdByTarget(OpenGLInfo.TYPE type, long eglContextId, int target) {
        return BindMap.getInstance().getBindInfo(type, eglContextId, target);
    }


}
