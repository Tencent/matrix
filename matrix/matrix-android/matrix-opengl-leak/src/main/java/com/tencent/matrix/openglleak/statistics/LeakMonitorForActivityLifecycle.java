package com.tencent.matrix.openglleak.statistics;

import android.app.Activity;
import android.app.Application;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;

import com.tencent.matrix.openglleak.statistics.source.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.source.ResRecorderForActivityLifecycle;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

@Deprecated
public class LeakMonitorForActivityLifecycle implements Application.ActivityLifecycleCallbacks {

    private static final String TAG = "matrix.GPU_LeakMonitor";
    private static LeakMonitorForActivityLifecycle mInstance = new LeakMonitorForActivityLifecycle();

    private Handler mH;

    private Map<WeakReference<Activity>, List<Integer>> maps = new HashMap<>();
    private String currentActivityName = "";

    private ResRecorderForActivityLifecycle mResRecorder = new ResRecorderForActivityLifecycle();

    private long mDoubleCheckTime = 1000 * 60 * 30;
    private final long mDoubleCheckLooper = 1000 * 60 * 1;

    protected LeakMonitorForActivityLifecycle() {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
        mH.postDelayed(doubleCheckRunnable, mDoubleCheckLooper);
    }

    public static LeakMonitorForActivityLifecycle getInstance() {
        return mInstance;
    }

    public void setDoubleCheckTime(long doubleCheckTime) {
        mDoubleCheckTime = doubleCheckTime;
    }

    public void setListener(LeakListener l) {
        mResRecorder.setLeakListener(l);
    }

    public void start(Application context) {
        MatrixLog.i(TAG, "start");

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            (context).registerActivityLifecycleCallbacks(mInstance);
        }
    }

    public String getCurrentActivityName() {
        return currentActivityName;
    }

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        currentActivityName = activity.getLocalClassName();

        MatrixLog.i(TAG, "activity oncreate:" + currentActivityName + "  :" + activity.hashCode());

        WeakReference<Activity> weakReference = new WeakReference<>(activity);
        List<Integer> actvityList = mResRecorder.getAllHashCode();

        maps.put(weakReference, actvityList);
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        MatrixLog.i(TAG,
                "activity ondestroy:" + activity.getLocalClassName() + "  :" + activity.hashCode());

        WeakReference<Activity> target = null;

        Set mapSet = maps.keySet();
        Iterator it = mapSet.iterator();
        while (it.hasNext()) {
            WeakReference<Activity> weakReference = (WeakReference) it.next();

            Activity a = (Activity) weakReference.get();
            if (a == null) {
                // 空的话移除
                it.remove();
                continue;
            }

            if ((a == activity)) {
                target = weakReference;
                break;
            }
        }

        // 找不到，跳过不处理
        if (target == null) {
            return;
        }

        String activityName = "";
        Activity aa = target.get();
        if (aa != null) {
            activityName = aa.getLocalClassName();
        }

        final String activityStr = activityName;
        final List<Integer> createList = maps.get(target);
        final List<Integer> destroyList = mResRecorder.getAllHashCode();

        mH.post(new Runnable() {
            @Override
            public void run() {
                boolean result = findLeak(createList, destroyList);
                if (result) {
                    MatrixLog.i(TAG, activityStr + " leak! ");
                }
            }
        });

        // clear
        maps.remove(target);
    }

    private boolean findLeak(List<Integer> createList, List<Integer> destroyList) {
        if ((createList == null) || (destroyList == null)) {
            return false;
        }

        boolean hasLeak = false;

        for (Integer destroy : destroyList) {
            if (destroy == null) {
                continue;
            }

            boolean isLeak;
            int i = 0;
            for (Integer create : createList) {
                if (create == null) {
                    i = i + 1;
                    continue;
                }

                if (create.intValue() == destroy.intValue()) {
                    break;
                }

                i = i + 1;
            }

            if (i == createList.size()) {
                isLeak = true;
            } else {
                isLeak = false;
            }

            if (isLeak) {
                OpenGLInfo leakInfo = mResRecorder.getItemByHashCode(destroy);

                if ((leakInfo != null) && !leakInfo.getMaybeLeak()) {
                    mResRecorder.getNativeStack(leakInfo);
                    mResRecorder.setMaybeLeak(leakInfo);

                    hasLeak = true;
                }
            }
        }

        return hasLeak;
    }

    @Override
    public void onActivityStarted(Activity activity) {

    }

    @Override
    public void onActivityResumed(Activity activity) {
        currentActivityName = activity.getLocalClassName();
    }

    @Override
    public void onActivityPaused(Activity activity) {

    }

    @Override
    public void onActivityStopped(Activity activity) {

    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

    }

    public interface LeakListener {
        void onMaybeLeak(OpenGLInfo info);

        void onLeak(OpenGLInfo info);
    }

    private Runnable doubleCheckRunnable = new Runnable() {
        @Override
        public void run() {
            long now = System.currentTimeMillis();

            List<OpenGLInfo> copyList = mResRecorder.getCopyList();
            MatrixLog.i(TAG, "double check list size:" + copyList.size());

            for (OpenGLInfo item : copyList) {
                if (item == null) {
                    continue;
                }

                if (item.getMaybeLeak()) {
                    if ((now - item.getMaybeLeakTime()) > mDoubleCheckTime) {
                        mResRecorder.setLeak(item);
                        mResRecorder.remove(item);
                    }
                }
            }

            mH.postDelayed(doubleCheckRunnable, mDoubleCheckLooper);
        }
    };
}
