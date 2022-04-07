package com.tencent.matrix.openglleak.statistics;

import android.app.Application;
import android.os.Handler;

import com.tencent.matrix.lifecycle.IStateObserver;
import com.tencent.matrix.lifecycle.owners.ProcessExplicitBackgroundOwner;
import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.resource.ResRecordManager;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;


public class LeakMonitorForBackstage extends LeakMonitorDefault implements IStateObserver {

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
        ProcessExplicitBackgroundOwner.INSTANCE.observeForever(this);
        super.start(context);
    }

    public void stop(Application context) {
        ProcessExplicitBackgroundOwner.INSTANCE.removeObserver(this);
        super.stop(context);
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
                                allInfos.add(item);
                            }
                        }
                        it.remove();
                    }
                }
                if (mLeakListListener != null) {
                    mLeakListListener.onLeak(allInfos);
                }
            }
        }
    };

    @Override
    public void on() {
        mH.postDelayed(mRunnable, mBackstageCheckTime);
    }

    @Override
    public void off() {
        mH.removeCallbacks(mRunnable);
    }

    public interface LeakListener {
        void onLeak(OpenGLInfo info);
    }

    public interface LeakListListener {
        void onLeak(List<OpenGLInfo> infos);
    }

}
