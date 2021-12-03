package com.tencent.matrix.openglleak.statistics.resource;

import com.tencent.matrix.openglleak.utils.ActivityRecorder;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;

public class OpenGLInfo {

    private String threadId = "";
    private final int id;
    private final long eglContextNativeHandle;
    private String javaStack = "";
    private String nativeStack = "";
    private long nativeStackPtr = 0L;
    private final TYPE type;
    private MemoryInfo memoryInfo;

    private ActivityRecorder.ActivityInfo activityInfo;

    private AtomicInteger counter;

    public enum TYPE {
        TEXTURE, BUFFER, FRAME_BUFFERS, RENDER_BUFFERS
    }

    public OpenGLInfo(OpenGLInfo clone) {
        this.threadId = clone.threadId;
        this.id = clone.id;
        this.eglContextNativeHandle = clone.eglContextNativeHandle;
        this.javaStack = clone.javaStack;
        this.nativeStack = clone.nativeStack;
        this.nativeStackPtr = clone.nativeStackPtr;
        this.type = clone.type;
        this.activityInfo = clone.activityInfo;
    }


    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle) {
        this.threadId = threadId;
        this.id = id;
        this.eglContextNativeHandle = eglContextNativeHandle;
        this.type = type;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle, String javaStack, long nativeStackPtr, ActivityRecorder.ActivityInfo activityInfo, AtomicInteger counter) {
        this.threadId = threadId;
        this.javaStack = javaStack;
        this.nativeStackPtr = nativeStackPtr;
        this.type = type;
        this.activityInfo = activityInfo;
        this.counter = counter;
        this.id = id;
        this.eglContextNativeHandle = eglContextNativeHandle;
    }

    public MemoryInfo getMemoryInfo() {
        return memoryInfo;
    }

    public long getEglContextNativeHandle() {
        return eglContextNativeHandle;
    }

    public void setMemoryInfo(MemoryInfo memoryInfo) {
        if (this.memoryInfo != null) {
            if (memoryInfo.getNativeStackPtr() != 0) {
                ResRecordManager.releaseNative(memoryInfo.getNativeStackPtr());
            }
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

    @Override
    public String toString() {
        return "OpenGLInfo{" +
                "id=" + id +
                ", activityName=" + activityInfo +
                ", type='" + type.toString() + '\'' +
                ", threadId='" + threadId + '\'' +
                ", eglContextNativeHandle='" + eglContextNativeHandle + '\'' +
                ", javaStack='" + javaStack + '\'' +
                ", nativeStack='" + getNativeStack() + '\'' +
                ", nativeStackPtr=" + nativeStackPtr +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || !(o instanceof OpenGLInfo)) return false;
        OpenGLInfo that = (OpenGLInfo) o;
        return id == that.id &&
                eglContextNativeHandle == that.eglContextNativeHandle &&
                threadId.equals(that.threadId) &&
                type == that.type;
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, eglContextNativeHandle, threadId, type);
    }

}
