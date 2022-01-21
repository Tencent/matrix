package com.tencent.matrix.openglleak.statistics.resource;

import android.opengl.EGLContext;
import android.os.Build;
import com.tencent.matrix.openglleak.utils.ActivityRecorder;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;

public class OpenGLInfo {

    private String threadId = "";
    private final int id;
    private String javaStack = "";
    private String nativeStack = "";
    private long nativeStackPtr = 0L;
    private final TYPE type;
    private MemoryInfo memoryInfo;
    private EGLContext eglContext;

    private ActivityRecorder.ActivityInfo activityInfo;

    private AtomicInteger counter;

    public enum TYPE {
        TEXTURE, BUFFER, FRAME_BUFFERS, RENDER_BUFFERS
    }

    public OpenGLInfo(OpenGLInfo clone) {
        this.threadId = clone.threadId;
        this.id = clone.id;
        this.eglContext = clone.eglContext;
        this.javaStack = clone.javaStack;
        this.nativeStack = clone.nativeStack;
        this.nativeStackPtr = clone.nativeStackPtr;
        this.type = clone.type;
        this.activityInfo = clone.activityInfo;
        this.memoryInfo = clone.memoryInfo;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, EGLContext eglContext) {
        this.threadId = threadId;
        this.id = id;
        this.eglContext = eglContext;
        this.type = type;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, EGLContext eglContext, String javaStack, long nativeStackPtr, ActivityRecorder.ActivityInfo activityInfo, AtomicInteger counter) {
        this.threadId = threadId;
        this.javaStack = javaStack;
        this.nativeStackPtr = nativeStackPtr;
        this.type = type;
        this.activityInfo = activityInfo;
        this.counter = counter;
        this.id = id;
        this.eglContext = eglContext;
    }

    public MemoryInfo getMemoryInfo() {
        return memoryInfo;
    }

    public EGLContext getEglContext() {
        return eglContext;
    }

    public long getEglContextNativeHandle() {
        long eglContextNativeHandle = 0L;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            if (null != eglContext) {
                eglContextNativeHandle = eglContext.getNativeHandle();
            }
        }

        return eglContextNativeHandle;
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
        return javaStack;
    }

    public String getNativeStack() {
        return ResRecordManager.getInstance().getNativeStack(this);
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

    @Override
    public String toString() {
        return "OpenGLInfo{" +
                "id=" + id +
                ", activityName=" + activityInfo +
                ", type='" + type.toString() + '\'' +
                ", threadId='" + threadId + '\'' +
                ", eglContextNativeHandle='" + getEglContextNativeHandle() + '\'' +
                ", eglContextReleased='" + isEglContextReleased() + '\'' +
                ", javaStack='" + javaStack + '\'' +
                ", nativeStack='" + getNativeStack() + '\'' +
                ", nativeStackPtr=" + nativeStackPtr +
                ", memoryInfo=" + memoryInfo +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || !(o instanceof OpenGLInfo)) return false;
        OpenGLInfo that = (OpenGLInfo) o;
        return id == that.id &&
                getEglContextNativeHandle() == that.getEglContextNativeHandle() &&
                threadId.equals(that.threadId) &&
                type == that.type;
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, getEglContextNativeHandle(), threadId, type);
    }

}
