package com.tencent.matrix.openglleak;

import com.tencent.matrix.openglleak.statistics.OpenGLInfo;

public interface OpenglReportCallback {

    void onGenOpenGLResource(OpenGLInfo info);

    void onDeleteOpenGLResource(OpenGLInfo info);

    void onBeforeIgnore(OpenGLInfo info);

    void onIgnoreGLResource(OpenGLInfo info);

    void onMaybeLeakTrigger(OpenGLInfo info);

    void onLeakTrigger(OpenGLInfo info);


}
