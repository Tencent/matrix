package com.tencent.matrix.openglleak.statistics.source;

import android.os.Handler;

import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.util.LinkedList;
import java.util.List;

public class ResRecordManager {

    private static ResRecordManager mInstance = new ResRecordManager();

    private Handler mH;

    private List<Callback> mCallbackList = new LinkedList<>();

    private ResRecordManager() {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
    }

    public static ResRecordManager getInstance() {
        return mInstance;
    }

    public void gen(final OpenGLInfo oinfo) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                if (oinfo == null) {
                    return;
                }

                for (Callback cb : mCallbackList) {
                    if (null != cb) {
                        cb.gen(oinfo);
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

                for (Callback cb : mCallbackList) {
                    if (null != cb) {
                        cb.delete(del);
                    }
                }
            }
        });
    }

    protected void registerCallback(Callback callback) {
        if (null == callback) {
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

    interface Callback {
        void gen(OpenGLInfo res);

        void delete(OpenGLInfo res);
    }

}
