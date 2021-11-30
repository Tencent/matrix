package com.tencent.matrix.openglleak.statistics;

import android.os.Handler;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.openglleak.statistics.source.OpenGLInfo;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.util.List;

public class LeakMonitorDefault extends CustomizeLeakMonitor implements IAppForeground {

    private static final long CHECK_TIME = 1000L * 30;

    private Handler mH;
    private LeakListener mLeakListener;

    private static LeakMonitorDefault mInstance = new LeakMonitorDefault();

    private LeakMonitorDefault() {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
    }

    public void setLeakListener(LeakListener l) {
        mLeakListener = l;
    }

    public void start() {
        AppActiveMatrixDelegate.INSTANCE.addListener(this);
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
            List<OpenGLInfo> leaks = checkEnd();

            if (null == mLeakListener) {
                return;
            }

            for (OpenGLInfo item : leaks) {
                if (null != item) {
                    mLeakListener.onLeak(item);
                }
            }
        }
    };

    public void foreground() {
        checkStart();
        mH.removeCallbacks(mRunnable);
    }

    public void background() {
        mH.postDelayed(mRunnable, CHECK_TIME);
    }

    public interface LeakListener {
        void onLeak(OpenGLInfo info);
    }
}
