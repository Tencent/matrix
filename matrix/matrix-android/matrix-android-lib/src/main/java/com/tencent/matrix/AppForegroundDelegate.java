package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentCallbacks2;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.util.ArrayMap;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;


public enum AppForegroundDelegate {

    INSTANCE;

    private static final String TAG = "Matrix.AppForegroundDelegate";
    private static final String TAG_CARE = "Matrix.AppForeground";

    private Set<IAppForeground> listeners = Collections.synchronizedSet(new HashSet<IAppForeground>());
    private boolean isAppForeground = false;
    private String visibleScene = "default";
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

    public String getVisibleScene() {
        return visibleScene;
    }

    private void onDispatchForeground(String visibleScene) {
        if (isAppForeground || !isInited) {
            return;
        }

        MatrixLog.i(TAG_CARE, "onForeground... visibleScene[%s]", visibleScene);
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
        MatrixLog.i(TAG_CARE, "onBackground... visibleScene[%s]", visibleScene);
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

        private FragmentManager.FragmentLifecycleCallbacks fragmentLifecycleCallbacks = new FragmentManager.FragmentLifecycleCallbacks() {

            @Override
            public void onFragmentResumed(@NonNull FragmentManager fm, @NonNull Fragment f) {
                super.onFragmentResumed(fm, f);
                MatrixLog.i(TAG, "[FragmentResumed] %s", visibleScene = getVisibleScene(f.getActivity()));
            }
        };

        @Override
        public void onActivityStarted(Activity activity) {
            if (!isAppForeground) {
                onDispatchForeground(visibleScene);
            }
        }


        @Override
        public void onActivityStopped(Activity activity) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (activity instanceof FragmentActivity) {
                    FragmentActivity supportActivity = (FragmentActivity) activity;
                    supportActivity.getSupportFragmentManager().unregisterFragmentLifecycleCallbacks(fragmentLifecycleCallbacks);

                }
            }
            if (getTopActivityName() == null) {
                onDispatchBackground(visibleScene);
            }
        }


        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (activity instanceof FragmentActivity) {
                    FragmentActivity supportActivity = (FragmentActivity) activity;
                    supportActivity.getSupportFragmentManager().registerFragmentLifecycleCallbacks(fragmentLifecycleCallbacks, true);

                }
            }
        }

        @Override
        public void onActivityResumed(Activity activity) {
            visibleScene = getVisibleScene(activity);
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
            if (level == TRIM_MEMORY_UI_HIDDEN && isAppForeground) { // fallback
                onDispatchBackground(visibleScene);
            }
        }
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
                    return activity.getClass().getCanonicalName();
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

    public static String getVisibleScene(Activity activity) {
        if (activity instanceof FragmentActivity) {
            FragmentActivity fragmentActivity = (FragmentActivity) activity;
            FragmentManager fragmentManager = fragmentActivity.getSupportFragmentManager();
            List<Fragment> fragments = fragmentManager.getFragments();
            List<Fragment> visibleFragments = new LinkedList<>();
            Rect rect = new Rect();
            if (fragments != null) {
                for (Fragment fragment : fragments) {
                    if (fragment != null && fragment.getView() != null) {
                        fragment.getView().getGlobalVisibleRect(rect);
                        long rate = rect.width() * rect.height();
                        if (rate > rect.width() || rate > rect.height()) {
                            visibleFragments.add(fragment);
                        }
                    }
                }
                StringBuilder ss = new StringBuilder(activity.getClass().getName());
                for (Fragment fragment : visibleFragments) {
                    ss.append("#").append(fragment.getClass().getSimpleName());
                }
                return ss.toString();
            }
        }
        return activity.getClass().getName();
    }


}
