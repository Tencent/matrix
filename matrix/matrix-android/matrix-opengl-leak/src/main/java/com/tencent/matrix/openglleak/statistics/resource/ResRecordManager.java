package com.tencent.matrix.openglleak.statistics.resource;

import android.os.Handler;

import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

public class ResRecordManager {

    private static ResRecordManager mInstance = new ResRecordManager();

    private Handler mH;

    private List<Callback> mCallbackList = new LinkedList<>();
    private List<OpenGLInfo> mInfoList = new LinkedList<>();

    private ResRecordManager() {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
    }

    public static ResRecordManager getInstance() {
        return mInstance;
    }

    public void gen(final OpenGLInfo gen) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                if (gen == null) {
                    return;
                }

                synchronized (mInfoList) {
                    mInfoList.add(gen);
                }

                for (Callback cb : mCallbackList) {
                    if (null != cb) {
                        cb.gen(gen);
                    }
                }
            }
        });
    }

    public void delete(final OpenGLInfo del) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                if (del == null) {
                    return;
                }

                synchronized (mInfoList) {
                    // 之前可能释放过
                    int index = mInfoList.indexOf(del);
                    if (-1 == index) {
                        return;
                    }

                    OpenGLInfo info = mInfoList.get(index);
                    if (null == info) {
                        return;
                    }

                    AtomicInteger counter = info.getCounter();
                    counter.set(counter.get() - 1);
                    if (counter.get() == 0) {
                        releaseNative(info.getNativeStackPtr());
                    }

                    mInfoList.remove(del);
                }

                for (Callback cb : mCallbackList) {
                    if (null != cb) {
                        cb.delete(del);
                    }
                }
            }
        });
    }

    public String getNativeStack(OpenGLInfo item) {
        synchronized (mInfoList) {
            // 之前可能释放过
            int index = mInfoList.indexOf(item);
            if (-1 == index) {
                return "res already released, can not get native stack";
            }

            String ret = "";

            OpenGLInfo info = mInfoList.get(index);
            long nativeStackPtr = info.getNativeStackPtr();
            if ((null != info) && (nativeStackPtr != 0L)) {
                ret = dumpNativeStack(nativeStackPtr);
            }

            return ret;
        }
    }

    private native String dumpNativeStack(long nativeStackPtr);

    private native void releaseNative(long nativeStackPtr);

    protected void registerCallback(Callback callback) {
        if (null == callback) {
            return;
        }

        if (mCallbackList.contains(callback)) {
            return;
        }

        mCallbackList.add(callback);
    }

    protected void unregisterCallback(Callback callback) {
        if (null == callback) {
            return;
        }
        mCallbackList.remove(callback);
    }

    public boolean isGLInfoRelease(OpenGLInfo item) {
        synchronized (mInfoList) {
            return !mInfoList.contains(item);
        }
    }

    public void reset() {
        mCallbackList.clear();
        mInfoList.clear();
    }

    interface Callback {
        void gen(OpenGLInfo res);

        void delete(OpenGLInfo res);
    }

}
