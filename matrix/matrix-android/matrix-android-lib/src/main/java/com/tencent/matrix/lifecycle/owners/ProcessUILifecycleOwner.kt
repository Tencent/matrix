package com.tencent.matrix.lifecycle.owners

import android.annotation.SuppressLint
import android.app.Activity
import android.app.ActivityManager
import android.app.Application
import android.content.ComponentName
import android.content.Context
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Process
import android.text.TextUtils
import androidx.lifecycle.*
import com.tencent.matrix.lifecycle.*
import com.tencent.matrix.listeners.IAppForeground
import com.tencent.matrix.util.*
import java.util.*

/**
 * Usage:
 * recommended readable APIs:
 *      ProcessUIStartedStateOwner.isForeground()
 *      ProcessUIStartedStateOwner.addLifecycleCallback(object : IMatrixLifecycleCallback() {
 *           override fun onForeground() {}
 *           override fun onBackground() {}
 *      })
 *      // auto remove callback when lifecycle destroyed
 *      ProcessUIStartedStateOwner.addLifecycleCallback(lifecycleOwner, object : IMatrixLifecycleCallback() {
 *           override fun onForeground() {}
 *           override fun onBackground() {}
 *      })
 *
 * the origin abstract APIs are also available:
 *      ProcessUIStartedStateOwner.active() // return true when state ON, in other words, it is foreground now
 *      ProcessUIStartedStateOwner.observeForever(object : IStateObserver {
 *          override fun on() {} // entered the state, in other words, turned foreground
 *          override fun off() {} // exit the state, in other words, turned background
 *      })
 *      // auto remove callback when lifecycle destroyed
 *      ProcessUIStartedStateOwner.observeForeverWithLifecycle(lifecycleOwner, object : IStateObserver {
 *          override fun on() {}
 *          override fun off() {}
 *      })
 *
 * State-ON:
 *  any Activity started
 * State-OFF:
 *  all Activity stopped
 */
object ProcessUIStartedStateOwner: IForegroundStatefulOwner by ProcessUILifecycleOwner.startedStateOwner

/**
 * API:
 * similar to [ProcessUIStartedStateOwner]
 *
 * State-ON:
 *  any Activity created
 * State-OFF:
 *  all Activity destroyed
 */
object ProcessUICreatedStateOwner: IForegroundStatefulOwner by ProcessUILifecycleOwner.createdStateOwner

/**
 * API:
 * similar to [ProcessUIStartedStateOwner]
 *
 * State-ON:
 *  any Activity resumed
 * State-OFF:
 *  all Activity paused
 */
object ProcessUIResumedStateOwner: IForegroundStatefulOwner by ProcessUILifecycleOwner.resumedStateOwner

/**
 * multi process version of [androidx.lifecycle.ProcessLifecycleOwner]
 *
 * for preloaded processes(never resumed or started), observer wouldn't be notified.
 * otherwise, adding observer would trigger callback immediately
 *
 * Activity's lifecycle callback is not always reliable and compatible. onStop might be called
 * without calling onStart or onResume, or onResume might be called more than once but stop once
 * only. And for some tabs or special devices, It is possible to show more than two Activity
 * at the same time.
 * For these unusual cases we don't use simple counter here.
 *
 * Created by Yves on 2021/9/14
 */
@SuppressLint("PrivateApi")
object ProcessUILifecycleOwner {

    private const val TAG = "Matrix.ProcessLifecycle"

    private const val TIMEOUT_MS = 500L //mls
    private var processName: String? = null
    private var packageName: String? = null
    private var activityManager: ActivityManager? = null
    private var activityInfoArray: Array<ActivityInfo>? = null

    internal fun init(app: Application) {
        activityManager = app.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        processName = MatrixUtil.getProcessName(app)
        packageName = MatrixUtil.getPackageName(app)
        activityInfoArray = safeLetOrNull(TAG) {
            app.packageManager
                .getPackageInfo(
                    packageName!!,
                    PackageManager.GET_ACTIVITIES
                ).activities
        }
        attach(app)
        MatrixLog.i(TAG, "init for [${processName}]")
    }

    // ========================== base ========================== //

    private val runningHandler = MatrixLifecycleThread.handler

    private val stub = Any()
    private val createdActivities = WeakHashMap<Activity, Any>()
    private val resumedActivities = WeakHashMap<Activity, Any>()
    private val startedActivities = WeakHashMap<Activity, Any>()

    private val destroyedActivities = WeakHashMap<Activity, Any>()

    private fun <R> WeakHashMap<Activity, Any>.synchronized(action: WeakHashMap<Activity, Any>.() -> R): R {
        synchronized(this) {
            return action()
        }
    }

    private fun WeakHashMap<Activity, Any>.put(activity: Activity) = put(activity, stub)

    private var pauseSent = true
    private var stopSent = true

    private open class AsyncOwner : StatefulOwner(), IForegroundStatefulOwner {
        open fun turnOnAsync() = turnOn()
        open fun turnOffAsync() = turnOff()
    }

    private class CreatedStateOwner : AsyncOwner() {
        override fun active(): Boolean {
            return super.active() && createdActivities.synchronized { all { false == it.key?.isFinishing } }
        }
    }

    internal val createdStateOwner: IForegroundStatefulOwner = CreatedStateOwner()
    internal val resumedStateOwner: IForegroundStatefulOwner = AsyncOwner()
    internal val startedStateOwner: IForegroundStatefulOwner = AsyncOwner()

    internal interface OnSceneChangedListener {
        fun onSceneChanged(newScene: String, origin: String)
    }

    internal var onSceneChangedListener: OnSceneChangedListener? = null
        internal set(value) {
            field = value
            if (value != null && startedStateOwner.active() && !TextUtils.isEmpty(recentScene)) {
                value.onSceneChanged(recentScene, "")
            }
        }

    var recentScene = ""
        private set(value) {
            onSceneChangedListener?.safeApply(TAG) {
                runningHandler.post { onSceneChanged(value, field) }
            }
            field = value
        }

    private val delayedPauseRunnable = Runnable {
        dispatchPauseIfNeeded()
        dispatchStopIfNeeded()
    }

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

    private fun activityCreated(activity: Activity) {
        val isEmptyBefore = createdActivities.isEmpty()
        createdActivities.synchronized {
            put(activity)
        }

        if (isEmptyBefore) {
            (createdStateOwner as AsyncOwner).turnOnAsync()
        }
    }

    private fun activityStarted(activity: Activity) {
        val isEmptyBefore = startedActivities.isEmpty()
        startedActivities.put(activity)

        if (isEmptyBefore && stopSent) {
            (startedStateOwner as AsyncOwner).turnOnAsync()
        }
    }

    private fun activityResumed(activity: Activity) {
        val isEmptyBefore = resumedActivities.isEmpty()
        resumedActivities.put(activity)
        if (isEmptyBefore) {
            if (pauseSent) {
                (resumedStateOwner as AsyncOwner).turnOnAsync()
                pauseSent = false
            } else {
                runningHandler.removeCallbacks(delayedPauseRunnable)
            }
        }
    }

    private fun activityPaused(activity: Activity) {
        resumedActivities.remove(activity)

        if (resumedActivities.isEmpty()) {
            runningHandler.postDelayed(delayedPauseRunnable, TIMEOUT_MS)
        }
    }

    private fun activityStopped(activity: Activity) {
        startedActivities.remove(activity)
        dispatchStopIfNeeded()
    }

    private fun activityDestroyed(activity: Activity) {
        createdActivities.synchronized {
            remove(activity)
            if (this.isEmpty()) {
                (createdStateOwner as AsyncOwner).turnOffAsync()
            }
        }
        destroyedActivities.put(activity)
        // fallback remove
        startedActivities.remove(activity)?.let {
            MatrixLog.w(
                TAG,
                "removed [$activity] when destroy, maybe something wrong with onStart/onStop callback"
            )
        }
        resumedActivities.remove(activity)?.let {
            MatrixLog.w(
                TAG,
                "removed [$activity] when destroy, maybe something wrong with onResume/onPause callback"
            )
        }
    }

    private fun dispatchPauseIfNeeded() {
        if (resumedActivities.isEmpty()) {
            pauseSent = true
            (resumedStateOwner as AsyncOwner).turnOffAsync()
        }
    }

    private fun dispatchStopIfNeeded() {
        if (startedActivities.isEmpty() && pauseSent) {
            stopSent = true
            (startedStateOwner as AsyncOwner).turnOffAsync()
        }
    }

    private fun attach(app: Application) {
        startedStateOwner.observeForever(DefaultLifecycleObserver())

        app.registerActivityLifecycleCallbacks(object : Application.ActivityLifecycleCallbacks {

            override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
                activityCreated(activity)
            }

            override fun onActivityStarted(activity: Activity) {
                recentScene = activity.javaClass.name
                updateScene(activity)
                activityStarted(activity)
            }

            override fun onActivityResumed(activity: Activity) {
                activityResumed(activity)
            }

            override fun onActivityPaused(activity: Activity) {
                activityPaused(activity)
            }

            override fun onActivityStopped(activity: Activity) {
                activityStopped(activity)
            }

            override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {
            }

            override fun onActivityDestroyed(activity: Activity) {
                activityDestroyed(activity)
            }
        })
    }

    private val componentToProcess by lazy { HashMap<String, String>() }

    private fun isComponentOfProcess(component: ComponentName?, process: String?): Boolean {
        if (component == null) {
            return false
        }

        if (component.packageName != packageName) {
            return false
        }

        if (activityInfoArray == null) {
            // init failed
            return true
        }

        return process == componentToProcess.getOrPut(component.className) {
            val info = activityInfoArray!!.find { it.name == component.className }
            if (info == null) {
                MatrixLog.e(TAG, "got task info not appeared in package manager $info")
                packageName!!
            } else {
                info.processName
            }
        }
    }

    private fun ActivityManager.RecentTaskInfo.belongsTo(processName: String?): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            val i = isComponentOfProcess(this.baseIntent.component, processName)
            val o = isComponentOfProcess(this.origActivity, processName)
            val b = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                isComponentOfProcess(this.baseActivity, processName)
            } else {
                false
            }
            val t = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                isComponentOfProcess(this.topActivity, processName)
            } else {
                false
            }

            i || o || b || t
        } else {
            false
        }
    }

    @JvmStatic
    fun hasRunningAppTask(): Boolean {
        if (activityManager == null) {
            throw IllegalStateException("NOT initialized yet")
        }
        return safeLet(TAG, defVal = true) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                activityManager!!.appTasks
                    .filter {
                        it.taskInfo.belongsTo(processName)
                    }.onEach {
                        MatrixLog.i(TAG, "$processName task: ${it.taskInfo.contentToString()}")
                    }.any {
                        MatrixLog.d(TAG, "hasRunningAppTask run any")
                        when {
                            Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q -> {
                                it.taskInfo.isRunning
                            }
                            Build.VERSION.SDK_INT >= Build.VERSION_CODES.M -> {
                                it.taskInfo.numActivities > 0
                            }
                            else -> {
                                @Suppress("DEPRECATION")
                                it.taskInfo.id == -1 // // If it is not running, this will be -1
                            }
                        }
                    }
            } else {
                false
            }
        }
    }

    @JvmStatic
    fun getRunningAppTasksOf(processName: String): Array<ActivityManager.AppTask> {
        if (activityManager == null) {
            throw IllegalStateException("NOT initialized yet")
        }
        return safeLet(TAG, defVal = emptyArray()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                activityManager!!.appTasks
                    .filter {
                        it.taskInfo.belongsTo(processName)
                    }.toTypedArray()
            } else {
                emptyArray()
            }
        }
    }

    // ========================== extension for compatibility ========================== //

    private val mListeners = HashSet<IAppForeground>()

    @get:JvmName("isProcessForeground")
    @Volatile
    var isProcessForeground = false
        private set

    // compat
    var visibleScene = "default"
        private set

    // compat
    var currentFragmentName: String? = null
        /**
         * must set after [Activity#onStart]
         */
        set(value) {
            MatrixLog.i(TAG, "[setCurrentFragmentName] fragmentName: $value")
            field = value

            if (value != null) {
                updateScene(value)
            } else {
                updateScene("?")
            }
        }

    fun addListener(listener: IAppForeground) {
        synchronized(mListeners) {
            mListeners.add(listener)
        }
    }

    fun removeListener(listener: IAppForeground) {
        synchronized(mListeners) {
            mListeners.remove(listener)
        }
    }

    private fun updateScene(activity: Activity) {
        visibleScene = activity.javaClass.name
    }

    private fun updateScene(scene: String) {
        visibleScene = scene
    }

    class DefaultLifecycleObserver : ISerialObserver {
        private fun onDispatchForeground() {
            if (isProcessForeground) {
                return
            }
            MatrixLog.i(TAG, "onForeground... visibleScene[$visibleScene@$processName]")
            MatrixLifecycleThread.executor.execute {
                isProcessForeground = true
                synchronized(mListeners) {
                    for (listener in mListeners) {
                        listener.onForeground(true)
                    }
                }
            }
        }

        private fun onDispatchBackground() {
            if (!isProcessForeground) {
                return
            }
            MatrixLog.i(TAG, "onBackground... visibleScene[$visibleScene@$processName]")
            MatrixLifecycleThread.executor.execute {
                isProcessForeground = false
                synchronized(mListeners) {
                    for (listener in mListeners) {
                        listener.onForeground(false)
                    }
                }
            }
        }

        override fun on() = onDispatchForeground()

        override fun off() = onDispatchBackground()
    }
}