package com.tencent.matrix.openglleak.statistics.resource;

import com.tencent.matrix.openglleak.utils.ActivityRecorder;
import com.tencent.matrix.openglleak.utils.JavaStacktrace;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;

public class OpenGLInfo {

    private String threadId = "";
    private final int id;
    private String nativeStack = "";
    private JavaStacktrace.Trace javatrace;
    private long nativeStackPtr = 0L;
    private final TYPE type;
    private MemoryInfo memoryInfo;
    private final long eglContext;
    private final long eglDrawSurface;
    private final long eglReadSurface;

    private ActivityRecorder.ActivityInfo activityInfo;

    private AtomicInteger counter;

    public enum TYPE {
        TEXTURE, BUFFER, FRAME_BUFFERS, RENDER_BUFFERS, EGL_CONTEXT
    }

    public OpenGLInfo(OpenGLInfo clone) {
        this.threadId = clone.threadId;
        this.id = clone.id;
        this.eglContext = clone.eglContext;
        this.javatrace = clone.javatrace;
        this.nativeStack = clone.nativeStack;
        this.nativeStackPtr = clone.nativeStackPtr;
        this.type = clone.type;
        this.activityInfo = clone.activityInfo;
        this.memoryInfo = clone.memoryInfo;
        this.eglDrawSurface = clone.eglDrawSurface;
        this.eglReadSurface = clone.eglReadSurface;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContext) {
        this.threadId = threadId;
        this.id = id;
        this.eglContext = eglContext;
        this.type = type;
        this.eglDrawSurface = 0;
        this.eglReadSurface = 0;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContext, long eglDrawSurface, long eglReadSurface, JavaStacktrace.Trace javatrace, long nativeStackPtr, ActivityRecorder.ActivityInfo activityInfo, AtomicInteger counter) {
        this.threadId = threadId;
        this.javatrace = javatrace;
        this.nativeStackPtr = nativeStackPtr;
        this.type = type;
        this.activityInfo = activityInfo;
        this.counter = counter;
        this.id = id;
        this.eglContext = eglContext;
        this.eglDrawSurface = eglDrawSurface;
        this.eglReadSurface = eglReadSurface;
    }

    public MemoryInfo getMemoryInfo() {
        return memoryInfo;
    }

    public long getEglContextNativeHandle() {
        return eglContext;
    }

    public void setMemoryInfo(MemoryInfo memoryInfo) {
        if (this.memoryInfo == memoryInfo) {
            return;
        }
        this.memoryInfo = memoryInfo;
    }

    public int getId() {
        return id;
    }

    public String getThreadId() {
        return threadId;
    }

    public TYPE getType() {
        return type;
    }

    public String getJavaStack() {
        if (javatrace == null) {
            return "";
        }
        return javatrace.getContent();
    }

    public String getNativeStack() {
        return ResRecordManager.getInstance().getNativeStack(this);
    }

    public String getBriefNativeStack() {
        return ResRecordManager.getInstance().getBriefNativeStack(this);
    }

    public AtomicInteger getCounter() {
        return counter;
    }

    public long getNativeStackPtr() {
        return nativeStackPtr;
    }

    public ActivityRecorder.ActivityInfo getActivityInfo() {
        return activityInfo;
    }

    public boolean isEglContextReleased() {
        return ResRecordManager.getInstance().isEglContextReleased(this);
    }

    public void releaseJavaStacktrace() {
        if (javatrace != null) {
            this.javatrace.reduceReference();
            this.javatrace = null;
        }
    }

    public long getEglDrawSurface() {
        return eglDrawSurface;
    }

    public long getEglReadSurface() {
        return eglReadSurface;
    }


    public boolean isEglSurfaceRelease() {
        return ResRecordManager.getInstance().isEglSurfaceReleased(this);
    }

    @Override
    public String toString() {
        return "OpenGLInfo{" +
                "id=" + id +
                ", activityName=" + activityInfo +
                ", type='" + type.toString() + '\'' +
                ", threadId='" + threadId + '\'' +
                ", eglContextNativeHandle='" + getEglContextNativeHandle() + '\'' +
                ", eglContextReleased='" + isEglContextReleased() + '\'' +
                ", eglSurfaceReleased='" + isEglSurfaceRelease() + '\'' +
                ", javaStack='" + getJavaStack() + '\'' +
                ", nativeStack='" + getNativeStack() + '\'' +
                ", nativeStackPtr=" + nativeStackPtr +
                ", memoryInfo=" + memoryInfo +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof OpenGLInfo)) return false;
        OpenGLInfo that = (OpenGLInfo) o;
        return id == that.id &&
                getEglContextNativeHandle() == that.getEglContextNativeHandle() &&
                /*threadId.equals(that.threadId) &&*/
                type == that.type;
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, getEglContextNativeHandle(), type);
    }

}
