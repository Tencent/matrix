package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentCallbacks2;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.ArrayMap;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;


public enum AppActiveMatrixDelegate {

    INSTANCE;

    private static final String TAG = "Matrix.AppActiveMatrixDelegate";
    private Set<IAppForeground> listeners = Collections.synchronizedSet(new HashSet<IAppForeground>());
    private boolean isAppForeground = false;
    private String visibleScene = "default";
    private Controller controller = new Controller();
    private boolean isInited = false;
    private String currentFragmentName;
    private String currentActivityName;

    public void init(Application application) {
        if (isInited) {
            MatrixLog.e(TAG, "has inited!");
            return;
        }
        this.isInited = true;
        application.registerComponentCallbacks(controller);
        application.registerActivityLifecycleCallbacks(controller);
    }

    public String getCurrentFragmentName() {
        return currentFragmentName;
    }

    /**
     * must set after {@link Activity#onStart()}
     * @param fragmentName
     */
    public void setCurrentFragmentName(String fragmentName) {
        MatrixLog.i(TAG, "[setCurrentFragmentName] fragmentName:%s", fragmentName);
        this.currentFragmentName = fragmentName;
        updateScene(fragmentName);
    }

    public String getVisibleScene() {
        return visibleScene;
    }

    private void onDispatchForeground(String visibleScene) {
        if (isAppForeground || !isInited) {
            return;
        }

        MatrixLog.i(TAG, "onForeground... visibleScene[%s]", visibleScene);
        try {
            for (IAppForeground listener : listeners) {
                listener.onForeground(true);
            }
        } finally {
            isAppForeground = true;
        }
    }

    private void onDispatchBackground(String visibleScene) {
        if (!isAppForeground || !isInited) {
            return;
        }
        MatrixLog.i(TAG, "onBackground... visibleScene[%s]", visibleScene);
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
            updateScene(activity);
            if (!isAppForeground) {
                onDispatchForeground(getVisibleScene());
            }
        }


        @Override
        public void onActivityStopped(Activity activity) {
            if (getTopActivityName() == null) {
                onDispatchBackground(getVisibleScene());
            }
        }


        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {

        }

        @Override
        public void onActivityDestroyed(Activity activity) {

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
        public void onConfigurationChanged(Configuration newConfig) {

        }

        @Override
        public void onLowMemory() {

        }

        @Override
        public void onTrimMemory(int level) {
            MatrixLog.i(TAG, "[onTrimMemory] level:%s", level);
            if (level == TRIM_MEMORY_UI_HIDDEN && isAppForeground) { // fallback
                onDispatchBackground(visibleScene);
            }
        }
    }

    private void updateScene(Activity activity) {
        visibleScene = currentActivityName = activity.getClass().getName();
    }

    private void updateScene(String currentFragmentName) {
        StringBuilder ss = new StringBuilder(currentActivityName == null ? "?" : currentActivityName);
        if (!TextUtils.isEmpty(currentFragmentName)) {
            ss.append("#").append(currentFragmentName);
        }
        visibleScene = ss.toString();
    }

    public static String getTopActivityName() {
        long start = System.currentTimeMillis();
        try {
            Class activityThreadClass = Class.forName("android.app.ActivityThread");
            Object activityThread = activityThreadClass.getMethod("currentActivityThread").invoke(null);
            Field activitiesField = activityThreadClass.getDeclaredField("mActivities");
            activitiesField.setAccessible(true);

            Map<Object, Object> activities;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
                activities = (HashMap<Object, Object>) activitiesField.get(activityThread);
            } else {
                activities = (ArrayMap<Object, Object>) activitiesField.get(activityThread);
            }
            if (activities.size() < 1) {
                return null;
            }
            for (Object activityRecord : activities.values()) {
                Class activityRecordClass = activityRecord.getClass();
                Field pausedField = activityRecordClass.getDeclaredField("paused");
                pausedField.setAccessible(true);
                if (!pausedField.getBoolean(activityRecord)) {
                    Field activityField = activityRecordClass.getDeclaredField("activity");
                    activityField.setAccessible(true);
                    Activity activity = (Activity) activityField.get(activityRecord);
                    return activity.getClass().getName();
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            long cost = System.currentTimeMillis() - start;
            MatrixLog.d(TAG, "[getTopActivityName] Cost:%s", cost);
        }
        return null;
    }

}
