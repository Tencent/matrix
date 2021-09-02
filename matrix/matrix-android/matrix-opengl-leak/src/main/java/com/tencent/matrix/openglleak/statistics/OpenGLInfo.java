package com.tencent.matrix.openglleak.statistics;

import android.text.TextUtils;

import java.util.concurrent.atomic.AtomicInteger;

public class OpenGLInfo {

    private static final String TAG = "OpenGLInfo.TAG";

    private int id;
    private int error;
    private String threadId = "";
    private long eglContextNativeHandle;
    private String javaStack = "";
    private String nativeStack = "";
    private long nativeStackPtr;
    private boolean genOrDelete;
    private TYPE type;

    private boolean isRelease;

    private boolean maybeLeak = false;
    private long maybeLeakCheckTime = 0L;
    private boolean isReported = false;

    private String activityName;

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
        this.isRelease = clone.isRelease;
        this.maybeLeakCheckTime = clone.maybeLeakCheckTime;
        this.isReported = clone.isReported;
        this.maybeLeak = clone.maybeLeak;
        this.activityName = clone.activityName;
        this.counter = clone.counter;
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

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle, String javaStack, long nativeStackPtr, boolean genOrDelete, String activityName, AtomicInteger counter) {
        this.id = id;
        this.threadId = threadId;
        this.eglContextNativeHandle = eglContextNativeHandle;
        this.javaStack = javaStack;
        this.nativeStackPtr = nativeStackPtr;
        this.genOrDelete = genOrDelete;
        this.type = type;
        this.activityName = activityName;
        this.counter = counter;
    }

    public int getId() {
        return id;
    }

    public int getError() {
        return error;
    }

    protected void setActivityName(String name) {
        this.activityName = name;
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
        if (TextUtils.isEmpty(nativeStack)) {
            if (!isRelease) {
                synchronized (counter) {
                    if (counter.get() > 0) {
                        if (this.nativeStackPtr != 0) {
                            nativeStack = dumpNativeStack(this.nativeStackPtr);
                        } else {
                            nativeStack = "";
                        }
                    }
                }
            }
        }
        return nativeStack;
    }

    public boolean getMaybeLeak() {
        return maybeLeak;
    }

    public String getActivityName() {
        return activityName;
    }

    public long getMaybeLeakTime() {
        return maybeLeakCheckTime;
    }

    public void setNativeStack(String nativeStack) {
        this.nativeStack = nativeStack;
    }

    public void setMaybeLeak(boolean b) {
        this.maybeLeak = b;
    }

    public void setMaybeLeakCheckTime(long time) {
        this.maybeLeakCheckTime = time;
    }

    public void release() {
        if (isRelease) {
            return;
        }
        isRelease = true;

        if (nativeStackPtr == 0) {
            return;
        }

        synchronized (counter) {
            int count = counter.get();
            if (count == 1) {
                releaseNative(nativeStackPtr);
            }

            counter.set(count - 1);
        }
    }

    private native void releaseNative(long nativeStackPtr);

    private native String dumpNativeStack(long nativeStackPtr);

    @Override
    public String toString() {
        return "OpenGLInfo{" +
                "id=" + id +
                ", activityName=" + activityName +
                ", type='" + type.toString() + '\'' +
                ", error=" + error +
                ", isGen=" + genOrDelete +
                ", threadId='" + threadId + '\'' +
                ", eglContextNativeHandle='" + eglContextNativeHandle + '\'' +
                ", javaStack='" + javaStack + '\'' +
                ", nativeStack='" + nativeStack + '\'' +
                ", nativeStackPtr=" + nativeStackPtr +
                '}';
    }

}
