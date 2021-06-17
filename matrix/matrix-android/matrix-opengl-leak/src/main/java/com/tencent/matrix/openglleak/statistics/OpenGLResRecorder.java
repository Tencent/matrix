package com.tencent.matrix.openglleak.statistics;

import android.os.Handler;
import android.os.HandlerThread;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class OpenGLResRecorder {

    private List<OpenGLInfo> infoList = new ArrayList<>();

    private static OpenGLResRecorder mInstance;

    private HandlerThread mHandlerThread;
    private Handler mH;

    private LeakMonitor.LeakListener mListener;

    private OpenGLResRecorder() {
        mHandlerThread = new HandlerThread("GpuResLeakMonitor");
        mHandlerThread.start();

        mH = new Handler(mHandlerThread.getLooper());
    }

    public static OpenGLResRecorder getInstance() {
        if (null == mInstance) {
            mInstance = new OpenGLResRecorder();
        }

        return mInstance;
    }

    public void setLeakListener(LeakMonitor.LeakListener mListener) {
        this.mListener = mListener;
    }

    public void gen(final OpenGLInfo oinfo) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                if (oinfo == null) {
                    return;
                }

                synchronized (infoList) {
                    infoList.add(oinfo);
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

                synchronized (infoList) {
                    Iterator<OpenGLInfo> iterator = infoList.iterator();
                    while (iterator.hasNext()) {
                        OpenGLInfo gen = iterator.next();
                        if (gen == null) {
                            continue;
                        }

                        if (gen.getType() == del.getType()) {
                            if (gen.getThreadId().equals(del.getThreadId())) {
                                if (gen.getId() == del.getId()) {
                                    gen.release();
                                    iterator.remove();
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        });
    }

    public List<OpenGLInfo> getCopyList() {
        List<OpenGLInfo> ll = new ArrayList<>();

        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                OpenGLInfo clone = new OpenGLInfo(item);
                ll.add(clone);
            }
        }

        return ll;
    }

    public void getNativeStack(OpenGLInfo info) {
        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                if (item.getType() == info.getType()) {
                    if (item.getThreadId().equals(info.getThreadId())) {
                        if (item.getId() == info.getId()) {
                            String stack = item.getNativeStack();
                            info.setNativeStack(stack);
                            return;
                        }
                    }
                }
            }
        }
    }

    public void setLeak(OpenGLInfo info) {
        synchronized (infoList) {
            Iterator<OpenGLInfo> iterator = infoList.iterator();

            while (iterator.hasNext()) {
                OpenGLInfo item = iterator.next();
                if (item == null) {
                    break;
                }

                if ((item.getType() == info.getType()) && (item.getThreadId().equals(info.getThreadId())) && (item.getId() == info.getId())) {
                    if (mListener != null) {
                        mListener.onLeak(item);
                    }

                    item.release();
                    iterator.remove();
                    return;
                }
            }
        }
    }

    public void setMaybeLeak(OpenGLInfo info) {
        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                if ((item.getType() == info.getType()) && (item.getThreadId().equals(info.getThreadId())) && (item.getId() == info.getId())) {
                    item.setMaybeLeak(true);
                    info.setMaybeLeak(true);

                    long now = System.currentTimeMillis();
                    item.setMaybeLeakCheckTime(now);
                    info.setMaybeLeakCheckTime(now);

                    if (mListener != null) {
                        mListener.onMaybeLeak(item);
                    }

                    return;
                }
            }
        }
    }

}
