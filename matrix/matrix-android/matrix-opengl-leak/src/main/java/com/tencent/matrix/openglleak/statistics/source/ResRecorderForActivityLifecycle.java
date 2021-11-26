package com.tencent.matrix.openglleak.statistics.source;

import android.os.Handler;

import com.tencent.matrix.openglleak.statistics.LeakMonitorForActivityLifecycle;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

@Deprecated
public class ResRecorderForActivityLifecycle implements ResRecordManager.Callback {

    private List<OpenGLInfo> infoList = new LinkedList<>();
    private Handler mH;

    private LeakMonitorForActivityLifecycle.LeakListener mListener;

    public ResRecorderForActivityLifecycle() {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
        ResRecordManager.getInstance().registerCallback(this);
    }

    public void setLeakListener(LeakMonitorForActivityLifecycle.LeakListener mListener) {
        this.mListener = mListener;
    }

    @Override
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

    @Override
    public void delete(final OpenGLInfo del) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                if (del == null) {
                    return;
                }

                synchronized (infoList) {
                    infoList.remove(del);
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

    public void remove(OpenGLInfo info) {
        if (infoList == null) {
            return;
        }
        synchronized (infoList) {
            infoList.remove(info);
        }
    }

    public List<Integer> getAllHashCode() {
        List<Integer> ll = new ArrayList<>();

        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                ll.add(item.hashCode());
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

    public OpenGLInfo getItemByHashCode(int hashCode) {
        synchronized (infoList) {
            for (OpenGLInfo item : infoList) {
                if (item == null) {
                    break;
                }

                if (item.hashCode() == hashCode) {
                    return item;
                }
            }
        }

        return null;
    }

}
