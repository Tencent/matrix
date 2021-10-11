package com.tencent.matrix.memory.canary.lifecycle.owners

import android.app.Activity
import android.os.Bundle
import com.tencent.matrix.memory.canary.DefaultMemoryCanaryInitializer
import com.tencent.matrix.memory.canary.lifecycle.StatefulOwner
import com.tencent.matrix.memory.canary.monitor.EmptyActivityLifecycleCallbacks
import com.tencent.matrix.util.MatrixLog
import java.util.*

/**
 * Created by Yves on 2021/9/24
 */
object ActivityRecord : StatefulOwner() {

    private const val TAG = "Matrix.memory.ActivityRecordOwner"

    private val callbacks = ActivityCallbacks()

    private val activityRecord = WeakHashMap<Activity, Any>()

    private fun Set<Activity>.contentToString(): String {

        val linkedList = LinkedList<String>()

        this.forEach {
            linkedList.add(it.javaClass.simpleName)
        }
        return "[${this.size}] $linkedList"
    }

    var currentActivity: String = "default"
        private set

    init {
        val app = DefaultMemoryCanaryInitializer.application!!
        app.registerActivityLifecycleCallbacks(callbacks)
    }

    private fun onStateChanged() {
        if (activityRecord.isEmpty()) {
            turnOff()
        } else {
            turnOn()
        }
    }

    private class ActivityCallbacks : EmptyActivityLifecycleCallbacks() {

        override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
            activityRecord[activity] = Any()
            onStateChanged()
            MatrixLog.d(
                TAG, "[${activity.javaClass.simpleName}] -> ${
                    activityRecord.keys.contentToString()
                }"
            )
        }

        override fun onActivityResumed(activity: Activity) {
            currentActivity = activity.javaClass.simpleName
        }

        override fun onActivityDestroyed(activity: Activity) {
            activityRecord.remove(activity)
            onStateChanged()
            MatrixLog.d(
                TAG, "[${activity.javaClass.simpleName}] <- ${
                    activityRecord.keys.contentToString()
                }"
            )
        }
    }
}