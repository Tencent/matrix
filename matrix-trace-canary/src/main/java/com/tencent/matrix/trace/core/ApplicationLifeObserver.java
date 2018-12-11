/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.tencent.matrix.trace.core;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;

import com.tencent.matrix.util.MatrixLog;

import java.lang.ref.WeakReference;
import java.util.LinkedList;

/**
 * Created by caichongyang on 17/5/18.
 * <p>
 * The Activity lifecycle observer class
 * </p>
 */
@SuppressWarnings("PMD")
public class ApplicationLifeObserver implements Application.ActivityLifecycleCallbacks {
    private static final String TAG = "Matrix.ApplicationLifeObserver";
    private static final long CHECK_DELAY = 600;
    private static ApplicationLifeObserver mInstance;
    private final LinkedList<IObserver> mObservers;
    private final Handler mMainHandler;
    private boolean mIsPaused, mIsForeground;
    private String mCurActivityHash;
    private Runnable mCheckRunnable;

    private ApplicationLifeObserver(@NonNull Application application) {
        if (null != application) {
            application.unregisterActivityLifecycleCallbacks(this);
            application.registerActivityLifecycleCallbacks(this);
        }
        mObservers = new LinkedList<>();
        mMainHandler = new Handler(Looper.getMainLooper());
    }

    public static void init(Application application) {
        if (null == mInstance) {
            mInstance = new ApplicationLifeObserver(application);
        }
    }

    public static ApplicationLifeObserver getInstance() {
        return mInstance;
    }

    public void register(IObserver observer) {
        if (null != mObservers) {
            mObservers.add(observer);
        }
    }

    public void unregister(IObserver observer) {
        if (null != mObservers) {
            mObservers.remove(observer);
        }
    }

    @Override
    public void onActivityResumed(final Activity activity) {
        for (IObserver listener : mObservers) {
            listener.onActivityResume(activity);
        }
        mIsPaused = false;
        final boolean wasBackground = !mIsForeground;
        mIsForeground = true;
        final String activityHash = getActivityHash(activity);

        if (!activityHash.equals(mCurActivityHash)) {
            for (IObserver listener : mObservers) {
                listener.onChange(activity, null);
            }
            mCurActivityHash = activityHash;
        }
        final WeakReference<Activity> mActivityWeakReference = new WeakReference<>(activity);
        mMainHandler.postDelayed(mCheckRunnable = new Runnable() {
            @Override
            public void run() {
                if (wasBackground) {
                    Activity ac = mActivityWeakReference.get();
                    if (null == ac) {
                        MatrixLog.w(TAG, "onFront ac is null!");
                        return;
                    }
                    for (IObserver listener : mObservers) {
                        listener.onFront(activity);
                    }
                }
            }
        }, CHECK_DELAY);
    }

    @Override
    public void onActivityPaused(final Activity activity) {
        for (IObserver listener : mObservers) {
            listener.onActivityPause(activity);
        }
        mIsPaused = true;
        if (mCheckRunnable != null) {
            mMainHandler.removeCallbacks(mCheckRunnable);
        }
        final WeakReference<Activity> mActivityWeakReference = new WeakReference<>(activity);
        mMainHandler.postDelayed(mCheckRunnable = new Runnable() {
            @Override
            public void run() {
                if (mIsForeground && mIsPaused) {
                    mIsForeground = false;
                    Activity ac = mActivityWeakReference.get();
                    if (null == ac) {
                        MatrixLog.w(TAG, "onBackground ac is null!");
                        return;
                    }
                    for (IObserver listener : mObservers) {
                        listener.onBackground(ac);
                    }
                }
            }
        }, CHECK_DELAY);
    }

    @Override
    public void onActivityCreated(final Activity activity, Bundle savedInstanceState) {
        for (IObserver listener : mObservers) {
            listener.onActivityCreated(activity);
        }
    }

    @Override
    public void onActivityStarted(Activity activity) {
        for (IObserver listener : mObservers) {
            listener.onActivityStarted(activity);
        }
    }

    @Override
    public void onActivityStopped(Activity activity) {
    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
    }

    @Override
    public void onActivityDestroyed(final Activity activity) {
        if (getActivityHash(activity).equals(mCurActivityHash)) {
            mCurActivityHash = null;
        }
    }

    public interface IObserver {
        void onFront(Activity activity);

        void onBackground(Activity activity);

        void onChange(Activity activity, Fragment fragment);

        void onActivityCreated(final Activity activity);

        void onActivityPause(final Activity activity);

        void onActivityResume(final Activity activity);

        void onActivityStarted(final Activity activity);
    }

    private String getActivityHash(Activity activity) {
        return activity.getClass().getName() + activity.hashCode();
    }


}
