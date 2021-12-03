package com.tencent.matrix.openglleak.statistics.resource;

import com.tencent.matrix.openglleak.utils.ActivityRecorder;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;

public class OpenGLInfo {

    private String threadId = "";
    private final OpenGLID openGLID;
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
        this.openGLID = clone.openGLID;
        this.javaStack = clone.javaStack;
        this.nativeStack = clone.nativeStack;
        this.nativeStackPtr = clone.nativeStackPtr;
        this.type = clone.type;
        this.activityInfo = clone.activityInfo;
    }


    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle, boolean genOrDelete) {
        this.threadId = threadId;
        this.openGLID = new OpenGLID(eglContextNativeHandle, id);
        this.type = type;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle, String javaStack, long nativeStackPtr, boolean genOrDelete, ActivityRecorder.ActivityInfo activityInfo, AtomicInteger counter) {
        this.threadId = threadId;
        this.javaStack = javaStack;
        this.nativeStackPtr = nativeStackPtr;
        this.type = type;
        this.activityInfo = activityInfo;
        this.counter = counter;
        this.openGLID = new OpenGLID(eglContextNativeHandle, id);
    }

    public OpenGLID getOpenGLID() {
        return openGLID;
    }

    public MemoryInfo getMemoryInfo() {
        return memoryInfo;
    }

    public long getEglContextNativeHandle() {
        return openGLID.getEglContextNativeHandle();
    }

    public void setMemoryInfo(MemoryInfo memoryInfo) {
        this.memoryInfo = memoryInfo;
    }

    public int getId() {
        return openGLID.getId();
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
                "id=" + openGLID.getId() +
                ", activityName=" + activityInfo +
                ", type='" + type.toString() + '\'' +
                ", threadId='" + threadId + '\'' +
                ", eglContextNativeHandle='" + openGLID.getEglContextNativeHandle() + '\'' +
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
        return  openGLID.equals(that.openGLID) &&
                threadId.equals(that.threadId) &&
                type == that.type;
    }

    @Override
    public int hashCode() {
        return Objects.hash(openGLID.hashCode(), threadId, type);
    }

}
