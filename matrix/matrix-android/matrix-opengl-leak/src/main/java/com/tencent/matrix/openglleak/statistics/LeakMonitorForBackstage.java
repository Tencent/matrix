package com.tencent.matrix.openglleak.statistics;

import android.app.Application;
import android.os.Handler;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.resource.ResRecordManager;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;


public class LeakMonitorForBackstage extends LeakMonitorDefault implements IAppForeground {

    private final long mBackstageCheckTime;

    private Handler mH;
    private LeakMonitorForBackstage.LeakListener mLeakListener;
    private LeakMonitorForBackstage.LeakListListener mLeakListListener;

    private List<OpenGLInfo> mLeaksList = new LinkedList<>();

    public LeakMonitorForBackstage(long backstageCheckTime) {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
        mBackstageCheckTime = backstageCheckTime;
    }

    @Override
    public void onLeak(OpenGLInfo leak) {
        synchronized (mLeaksList) {
            mLeaksList.add(leak);
        }
    }

    public void setLeakListener(LeakListener l) {
        mLeakListener = l;
    }

    public void setLeakListListener(LeakListListener l) {
        mLeakListListener = l;
    }

    public void start(Application context) {
        AppActiveMatrixDelegate.INSTANCE.addListener(this);
        super.start(context);
    }

    public void stop(Application context) {
        AppActiveMatrixDelegate.INSTANCE.removeListener(this);
        super.stop(context);
    }

    @Override
    public void onForeground(boolean isForeground) {
        if (isForeground) {
            foreground();
        } else {
            background();
        }
    }

    private Runnable mRunnable = new Runnable() {
        @Override
        public void run() {
            synchronized (mLeaksList) {
                List<OpenGLInfo> allInfos = new LinkedList<>();
                Iterator<OpenGLInfo> it = mLeaksList.iterator();
                while (it.hasNext()) {
                    OpenGLInfo item = it.next();
                    if (null != item) {
                        if (null != mLeakListener) {
                            if (!ResRecordManager.getInstance().isGLInfoRelease(item)) {
                                mLeakListener.onLeak(item);
                            }
                        }
                        allInfos.add(item);
                        it.remove();
                    }
                }
                if (mLeakListListener != null) {
                    mLeakListListener.onLeak(allInfos);
                }
            }
        }
    };

    public void foreground() {
        mH.removeCallbacks(mRunnable);
    }

    public void background() {
        mH.postDelayed(mRunnable, mBackstageCheckTime);
    }

    public interface LeakListener {
        void onLeak(OpenGLInfo info);
    }

    public interface LeakListListener {
        void onLeak(List<OpenGLInfo> infos);
    }

}
