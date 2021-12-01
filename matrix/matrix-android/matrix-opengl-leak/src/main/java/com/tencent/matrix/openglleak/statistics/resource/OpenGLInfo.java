package com.tencent.matrix.openglleak.statistics.resource;

import com.tencent.matrix.openglleak.utils.ActivityRecorder;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicInteger;

public class OpenGLInfo {

    private int id;
    private int error;
    private String threadId = "";
    private long eglContextNativeHandle;
    private String javaStack = "";
    private String nativeStack = "";
    private long nativeStackPtr = 0L;
    private boolean genOrDelete;
    private TYPE type;

    private ActivityRecorder.ActivityInfo activityInfo;

    private AtomicInteger counter;

    public enum TYPE {
        TEXTURE, BUFFER, FRAME_BUFFERS, RENDER_BUFFERS
    }

    public OpenGLInfo(OpenGLInfo clone) {
        this.id = clone.id;
        this.error = clone.error;
        this.threadId = clone.threadId;
        this.eglContextNativeHandle = clone.eglContextNativeHandle;
        this.javaStack = clone.javaStack;
        this.nativeStack = clone.nativeStack;
        this.nativeStackPtr = clone.nativeStackPtr;
        this.genOrDelete = clone.genOrDelete;
        this.type = clone.type;
        this.activityInfo = clone.activityInfo;
    }

    public OpenGLInfo(int error) {
        this.error = error;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle, boolean genOrDelete) {
        this.id = id;
        this.threadId = threadId;
        this.eglContextNativeHandle = eglContextNativeHandle;
        this.genOrDelete = genOrDelete;
        this.type = type;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle, String javaStack, long nativeStackPtr, boolean genOrDelete, ActivityRecorder.ActivityInfo activityInfo, AtomicInteger counter) {
        this.id = id;
        this.threadId = threadId;
        this.eglContextNativeHandle = eglContextNativeHandle;
        this.javaStack = javaStack;
        this.nativeStackPtr = nativeStackPtr;
        this.genOrDelete = genOrDelete;
        this.type = type;
        this.activityInfo = activityInfo;
        this.counter = counter;
    }

    public int getId() {
        return id;
    }

    public int getError() {
        return error;
    }

    public String getThreadId() {
        return threadId;
    }

    public long getEglContextNativeHandle() {
        return eglContextNativeHandle;
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
                ", error=" + error +
                ", isGen=" + genOrDelete +
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
        return Objects.hash(id, threadId, eglContextNativeHandle, type);
    }

}
