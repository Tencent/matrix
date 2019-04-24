package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentCallbacks2;
import android.content.res.Configuration;
import android.os.Bundle;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.util.MatrixLog;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;


public enum AppForegroundDelegate {

    INSTANCE;

    private static final String TAG = "Matrix.AppForegroundDelegate";
    private static final String TAG_CARE = "Matrix.AppForeground";

    private Set<IAppForeground> listeners = Collections.synchronizedSet(new HashSet<IAppForeground>());
    private boolean isAppForeground = false;
    private String foregroundActivity = "default";
    private Controller controller = new Controller();
    private boolean isInited = false;

    public void init(Application application) {
        if (isInited) {
            MatrixLog.e(TAG, "has inited!");
            return;
        }
        this.isInited = true;
        application.registerComponentCallbacks(controller);
        application.registerActivityLifecycleCallbacks(controller);

    }

    public String getForegroundActivity() {
        return foregroundActivity;
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


    private final class Controller implements Application.ActivityLifecycleCallbacks, ComponentCallbacks2 {


        @Override
        public void onActivityStarted(Activity activity) {
            if (!isAppForeground) {
                onDispatchForeground(activity.getClass().getCanonicalName());
            }
        }


        @Override
        public void onActivityStopped(Activity activity) {

        }


        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {

        }

        @Override
        public void onActivityResumed(Activity activity) {
            foregroundActivity = activity.getClass().getCanonicalName();
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


        @Override
        public void onConfigurationChanged(Configuration newConfig) {

        }

        @Override
        public void onLowMemory() {

        }

        @Override
        public void onTrimMemory(int level) {
            MatrixLog.i(TAG, "[onTrimMemory] level:%s", level);
            if (level == TRIM_MEMORY_UI_HIDDEN) {
                onDispatchBackground(foregroundActivity);
            }
        }
    }


}
