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
data class ActivityRecorderConfig(
    val baseActivities: List<String> = emptyList()
)

object ActivityRecorder : StatefulOwner() {

    private const val TAG = "Matrix.memory.ActivityRecorder"

    private val callbacks = ActivityCallbacks()

    private val stub = Any()

    private val createdActivities = WeakHashMap<Activity, Any>()
    private val destroyedActivities = WeakHashMap<Activity, Any>()

    private var baseActivities: List<String> = emptyList()

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

    internal fun init(app: Application, baseActivities: List<String>) {
        if (inited) {
            return
        }
        inited = true
        this.baseActivities = baseActivities
        app.registerActivityLifecycleCallbacks(callbacks)
    }

    private fun sizeExceptBase(): Int {
        var size = createdActivities.size
        baseActivities.forEach {
            if (createdActivities.contains(it)) {
                size--
            }
        }
        return size
    }

    private fun WeakHashMap<Activity, Any>.anyNotFinishedExceptBase(): Boolean {
        if (isEmpty()) return false

        val cpy = createdActivities.entries.toMutableList()

        cpy.removeAll {
            baseActivities.contains(it.key?.javaClass?.name)
        }

        if (cpy.isEmpty()) return false
        for (element in cpy) {
            val k = element.key
            if (k != null && !k.isFinishing) {
                return true
            }
        }
        return false
    }

    val staged: Boolean
        get() = active() && (sizeExceptBase() > 0 || createdActivities.anyNotFinishedExceptBase())

    fun retainedActivities(): Map<String, Int> {
        val map = HashMap<String, Int>()
        Runtime.getRuntime().gc()

        val cpy = destroyedActivities.entries.toTypedArray()

        for (e in cpy) {
            val k = e.key ?: continue
            k.javaClass.simpleName.let {
                var count = map.getOrPut(it, { 0 })
                map[it] = ++count
            }
        }

        return map
    }

    private fun onStateChanged() {
        if (createdActivities.isEmpty()) {
            turnOff()
        } else {
            turnOn()
        }
    }

    private class ActivityCallbacks : EmptyActivityLifecycleCallbacks() {

        override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
            createdActivities[activity] = stub
            onStateChanged()
            MatrixLog.d(
                TAG, "[${activity.javaClass.simpleName}] -> ${
                    createdActivities.keys.contentToString()
                }"
            )
        }

        override fun onActivityResumed(activity: Activity) {
            currentActivity = activity.javaClass.simpleName
        }

        override fun onActivityDestroyed(activity: Activity) {
            createdActivities.remove(activity)
            onStateChanged()
            MatrixLog.d(
                TAG, "[${activity.javaClass.simpleName}] <- ${
                    createdActivities.keys.contentToString()
                }"
            )
            destroyedActivities[activity] = stub
        }
    }
}