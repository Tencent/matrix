package com.tencent.matrix.openglleak.statistics;

import com.tencent.matrix.openglleak.statistics.resource.OpenGLID;
import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;

public class BindCenter {

    private static final String TAG = "matrix.BindCenter";

    private static final BindCenter mInstance = new BindCenter();

    public static BindCenter getInstance() {
        return mInstance;
    }

    private BindCenter() {
    }

    public void glBindResource(OpenGLInfo.TYPE type, int target, long eglContextId, int id) {
        BindMap.getInstance().putBindInfo(type, target, new OpenGLID(eglContextId, id));
    }

    public void glBindResource(OpenGLInfo.TYPE type, int target, OpenGLID openGLID) {
        BindMap.getInstance().putBindInfo(type, target, openGLID);
    }

    public OpenGLID findCurrentResourceIdByTarget(OpenGLInfo.TYPE type, long eglContextId, int target) {
        return BindMap.getInstance().getBindInfo(type, eglContextId, target);
    }


}
