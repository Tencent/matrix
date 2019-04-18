package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.util.MatrixLog;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;


/**
 * 应用前后台监控
 * <p>
 * 注意：
 * 1、如果应用在前台时，启动了另外一个进程，新启动的进程会接收切到前台的事件；
 * 2、只要是属于微信进程内的任意一个界面在前台，则认为当前应用处于前台，不单纯针对某个进程；
 * 3、前后台事件通知，所有进程都会接收到；
 */
public enum AppForegroundDelegate {

    INSTANCE;

    private static final String TAG = "Matrix.AppForegroundDelegate";
    private static final String TAG_CARE = "Matrix.AppForeground";

    private Set<IAppForeground> listeners = Collections.synchronizedSet(new HashSet<IAppForeground>());
    private boolean isAppForeground = false;
    private Controller controller = new Controller();
    private boolean isInited = false;

    public void init(Application application) {
        if (isInited) {
            MatrixLog.e(TAG, "has inited!");
            return;
        }
        this.isInited = true;

        application.registerActivityLifecycleCallbacks(controller);

    }


    private void onDispatchForeground(String activity) {
        if (isAppForeground || !isInited) {
            return;
        }

        MatrixLog.i(TAG_CARE, "onForeground... activity[%s]", activity);
        try {
            for (IAppForeground listener : listeners) {
                listener.onForeground(true);
            }
        } finally {
            isAppForeground = true;
        }
    }

    private void onDispatchBackground(String activity) {
        if (!isAppForeground || !isInited) {
            return;
        }
        MatrixLog.i(TAG_CARE, "onBackground... activity[%s]", activity);
        try {
            for (IAppForeground listener : listeners) {
                listener.onForeground(false);
            }
        } finally {
            isAppForeground = false;
        }
    }

    public boolean isAppForeground() {
        return isAppForeground;
    }

    public void addListener(IAppForeground listener) {
        listeners.add(listener);
    }

    public void removeListener(IAppForeground listener) {
        listeners.remove(listener);
    }


    private final class Controller implements Application.ActivityLifecycleCallbacks {

        private int activeCount = 0;
        private int ignoreCount = 0;


        @Override
        public void onActivityStarted(Activity activity) {
            if (activeCount <= 0) {
                onDispatchForeground(activity.getClass().getName());
            }
            if (ignoreCount < 0) {
                MatrixLog.w(TAG, "[onActivityStarted] activity[%s] has changed Configurations!", activity.getClass().getName());
                ignoreCount++;
            } else {
                activeCount++;
            }
        }


        @Override
        public void onActivityStopped(Activity activity) {
            if (null != activity && activity.isChangingConfigurations()) {
                MatrixLog.w(TAG, "[onActivityStopped] activity[%s] is changing Configurations!", activity.getClass().getName());
                ignoreCount--;
            } else {
                activeCount--;
                if (activeCount <= 0) {
                    onDispatchBackground(activity.getClass().getName());
                    activeCount = 0; // fix status
                }
            }
        }

        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {

        }

        @Override
        public void onActivityResumed(Activity activity) {

        }

        @Override
        public void onActivityPaused(Activity activity) {

        }

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

        }

        @Override
        public void onActivityDestroyed(Activity activity) {

        }


    }


}
