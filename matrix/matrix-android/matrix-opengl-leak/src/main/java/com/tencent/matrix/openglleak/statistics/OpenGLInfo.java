package com.tencent.matrix.openglleak.statistics;

import android.text.TextUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

public class OpenGLInfo {

    private static final String TAG = "OpenGLInfo.TAG";

    public static final int OP_GEN = 0x6565;
    public static final int OP_DELETE = 0x6566;
    public static final int OP_BIND = 0x6567;

    private int id;
    private int error;
    private String threadId = "";
    private long eglContextNativeHandle;
    private String javaStack = "";
    private String nativeStack = "";
    private long nativeStackPtr;
    private int operate;
    private TYPE type;
    // use to dump
    private int allocCount = 1;
    private List<Integer> idList = new ArrayList<>();

    private boolean isRelease;

    private boolean maybeLeak = false;
    private long maybeLeakCheckTime = 0L;
    private boolean isReported = false;

    private String activityName;

    private MemoryInfo memoryInfo;

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
        this.operate = clone.operate;
        this.type = clone.type;
        this.isRelease = clone.isRelease;
        this.maybeLeakCheckTime = clone.maybeLeakCheckTime;
        this.isReported = clone.isReported;
        this.maybeLeak = clone.maybeLeak;
        this.activityName = clone.activityName;
        this.counter = clone.counter;
        this.memoryInfo = clone.memoryInfo;
    }

    public OpenGLInfo(int error) {
        this.error = error;
    }

    public OpenGLInfo(TYPE type) {
        this.type = type;
    }

    public OpenGLInfo(TYPE type, int id, long eglContextNativeHandle, int operate) {
        this.id = id;
        this.eglContextNativeHandle = eglContextNativeHandle;
        this.operate = operate;
        this.type = type;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle, int operate) {
        this.id = id;
        this.threadId = threadId;
        this.eglContextNativeHandle = eglContextNativeHandle;
        this.operate = operate;
        this.type = type;
    }

    public OpenGLInfo(TYPE type, int id, String threadId, long eglContextNativeHandle, String javaStack, long nativeStackPtr, int operate, String activityName, AtomicInteger counter) {
        this.id = id;
        this.threadId = threadId;
        this.eglContextNativeHandle = eglContextNativeHandle;
        this.javaStack = javaStack;
        this.nativeStackPtr = nativeStackPtr;
        this.operate = operate;
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


    public int getAllocCount() {
        return allocCount;
    }

    public String getAllocIdList() {
        return idList.toString();
    }

    public void incAllocRecord(int id) {
        this.allocCount++;
        this.idList.add(id);
    }

    public MemoryInfo getMemoryInfo() {
        return memoryInfo;
    }

    public void setMemoryInfo(MemoryInfo memoryInfo) {
        this.memoryInfo = memoryInfo;
    }

    private String getOpStr() {
        if (operate == OP_GEN) {
            return "gen";
        }
        if (operate == OP_DELETE) {
            return "delete";
        }
        if (operate == OP_BIND) {
            return "bind";
        }
        return "unkown";
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

    public static native String dumpNativeStack(long nativeStackPtr);

    @Override
    public String toString() {
        return "OpenGLInfo{" +
                "id=" + id +
                ", eglContextNativeHandle='" + eglContextNativeHandle + '\'' +
                ", activityName=" + activityName +
                ", type='" + type.toString() + '\'' +
                ", threadId='" + threadId + '\'' +
                ", javaStack='" + javaStack + '\'' +
                ", nativeStack='" + nativeStack + '\'' +
                ", nativeStackPtr=" + nativeStackPtr +
                "\n memoryInfo=" + memoryInfo +
                '}';
    }

}
