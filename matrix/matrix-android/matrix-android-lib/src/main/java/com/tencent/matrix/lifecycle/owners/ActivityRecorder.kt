package com.tencent.matrix.lifecycle.owners

import android.app.Activity
import android.app.Application
import android.os.Bundle
import com.tencent.matrix.lifecycle.EmptyActivityLifecycleCallbacks
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.MatrixLog
import java.util.*
import kotlin.collections.HashMap

/**
 * Created by Yves on 2021/9/24
 */
object ActivityRecorder : StatefulOwner() {

    private const val TAG = "Matrix.memory.ActivityRecorder"

    private val callbacks = ActivityCallbacks()

    private val stub = Any()

    private val activityRecord = WeakHashMap<Activity, Any>()
    private val destroyedActivities = WeakHashMap<Activity, Any>()

    var baseActivities: Array<String> = emptyArray()
        set(value) {
            if (field.isNotEmpty()) {
                throw IllegalArgumentException("avoid resetting base Activities")
            }
            field = value
        }

    private fun WeakHashMap<Activity, Any>.contains(clazz: String): Boolean {
        entries.toTypedArray().forEach { e ->
            if (clazz == e.key?.javaClass?.name) {
                return true
            }
        }
        return false
    }

    private fun Set<Activity>.contentToString(): String {

        val linkedList = LinkedList<String>()

        this.forEach {
            linkedList.add(it.javaClass.simpleName)
        }
        return "[${this.size}] $linkedList"
    }

    var currentActivity: String = "default"
        private set

    @Volatile
    private var inited = false

    fun init(app: Application) {
        if (inited) {
            return
        }
        inited = true
        app.registerActivityLifecycleCallbacks(callbacks)
    }

    fun sizeExcept(activityNames: Array<String>?): Int {
        var size = activityRecord.size
        activityNames?.forEach {
            if (activityRecord.contains(it)) {
                size--
            }
        }
        return size
    }

    fun busy() = active() && sizeExcept(baseActivities) > 0

    fun retainedActivities(): Map<String, Int> {
        val map = HashMap<String, Int>()
        Runtime.getRuntime().gc()

        destroyedActivities.entries.toTypedArray()
            .filter {
                it.key != null
            }.forEach {
                it.key.javaClass.simpleName.let { name ->
                    var count = map.getOrPut(name, { 0 })
                    map[name] = ++count
                }
            }

        return map
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
            activityRecord[activity] = stub
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
            destroyedActivities[activity] = stub
        }
    }
}