package com.tencent.matrix.memory.canary.lifecycle

import android.app.Activity
import android.app.Application
import android.os.Bundle

/**
 * Created by Yves on 2021/9/22
 */
open class EmptyActivityLifecycleCallbacks : Application.ActivityLifecycleCallbacks{
    override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
    }

    override fun onActivityStarted(activity: Activity) {
    }

    override fun onActivityResumed(activity: Activity) {
    }

    override fun onActivityPaused(activity: Activity) {
    }

    override fun onActivityStopped(activity: Activity) {
    }

    override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {
    }

    override fun onActivityDestroyed(activity: Activity) {
    }
}