package com.tencent.matrix.lifecycle.owners

import android.app.Activity
import android.app.Application
import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import androidx.annotation.NonNull
import androidx.lifecycle.*
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.lifecycle.owners.MultiProcessLifecycleInitializer.Companion.init
import com.tencent.matrix.listeners.IAppForeground
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import java.util.*

/**
 * multi process version of [androidx.lifecycle.ProcessLifecycleOwner]
 *
 * for preloaded processes(never resumed or started), observer wouldn't be notified.
 * otherwise, adding observer would trigger callback immediately
 *
 * Activity's lifecycle callback is not always reliable and compatible. onStop might be called
 * without calling onStart or onResume, or onResume might be called more than once but stop once
 * only. For these wired cases we don't use simple counter here.
 *
 * Created by Yves on 2021/9/14
 */
object MultiProcessLifecycleOwner {

    private const val TAG = "Matrix.MultiProcessLifecycle"

    private var sProcessName: String? = null
    private const val TIMEOUT_MS = 500L //mls

    internal fun init(context: Context) {
        sProcessName = MatrixUtil.getProcessName(context)
        attach(context)
        MatrixLog.i(TAG, "init for [${sProcessName}]")
    }

    // ========================== base ========================== //

    private val stub = Any()
    private val resumedActivities = WeakHashMap<Activity, Any>()
    private val startedActivities = WeakHashMap<Activity, Any>()

    private fun WeakHashMap<Activity, Any>.put(activity: Activity) {
        put(activity, stub)
    }

    private var mPauseSent = true
    private var mStopSent = true

    private val runningHandler = Handler(MatrixHandlerThread.getDefaultHandlerThread().looper)

    private class AsyncOwner : StatefulOwner() {
        fun turnOnAsync() = runningHandler.post { turnOn() }
        fun turnOffAsync() = runningHandler.post { turnOff() }
    }

    val resumedStateOwner: StatefulOwner = AsyncOwner()
    val startedStateOwner: StatefulOwner = AsyncOwner()

    private val mDelayedPauseRunnable = Runnable {
        dispatchPauseIfNeeded()
        dispatchStopIfNeeded()
    }

    private fun activityStarted(activity: Activity) {
        val isEmptyBefore = startedActivities.isEmpty()
        startedActivities.put(activity)

        if (isEmptyBefore && mStopSent) {
            (startedStateOwner as AsyncOwner).turnOnAsync()
        }
    }

    private fun activityResumed(activity: Activity) {
        val isEmptyBefore = resumedActivities.isEmpty()
        resumedActivities.put(activity)
        if (isEmptyBefore) {
            if (mPauseSent) {
                (resumedStateOwner as AsyncOwner).turnOnAsync()
                mPauseSent = false
            } else {
                runningHandler.removeCallbacks(mDelayedPauseRunnable)
            }
        }
    }

    private fun activityPaused(activity: Activity) {
        resumedActivities.remove(activity)

        if (resumedActivities.isEmpty()) {
            runningHandler.postDelayed(mDelayedPauseRunnable, TIMEOUT_MS)
        }
    }

    private fun activityStopped(activity: Activity) {
        startedActivities.remove(activity)
        dispatchStopIfNeeded()
    }

    // fallback remove
    private fun activityDestroyed(activity: Activity) {
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
            mPauseSent = true
            (resumedStateOwner as AsyncOwner).turnOffAsync()
        }
    }

    private fun dispatchStopIfNeeded() {
        if (startedActivities.isEmpty() && mPauseSent) {
            mStopSent = true
            (startedStateOwner as AsyncOwner).turnOffAsync()
        }
    }

    private fun attach(context: Context) {
        startedStateOwner.observeForever(DefaultLifecycleObserver())

        val app = context.applicationContext as Application
        app.registerActivityLifecycleCallbacks(object : Application.ActivityLifecycleCallbacks {

            override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
            }

            override fun onActivityStarted(activity: Activity) {
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

    // ========================== extension ========================== //

    private val mListeners = HashSet<IAppForeground>()

    @get:JvmName("isProcessForeground")
    @Volatile
    var isProcessForeground = false
        private set

    var visibleScene = "default"
        private set

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

    class DefaultLifecycleObserver : IStateObserver {
        private fun onDispatchForeground() {
            if (isProcessForeground) {
                return
            }
            MatrixLog.i(TAG, "onForeground... visibleScene[$visibleScene@$sProcessName]")
            runningHandler.post {
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
            MatrixLog.i(TAG, "onBackground... visibleScene[$visibleScene@$sProcessName]")
            runningHandler.post {
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

/**
 * You should init [com.tencent.matrix.Matrix] or call [init] manually before creating any Activity
 * Created by Yves on 2021/9/14
 */
class MultiProcessLifecycleInitializer : ContentProvider() {

    companion object {

        @Volatile
        private var inited = false

        @JvmStatic
        fun init(@NonNull context: Context) {
            if (inited) {
                return
            }
            inited = true
            MultiProcessLifecycleOwner.init(context)
            ActivityRecorder.init(context.applicationContext as Application)
        }
    }

    override fun onCreate(): Boolean {
        if (context == null) {
            throw java.lang.IllegalStateException("context is null !!!")
        }

        init(context!!)

        return true
    }

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?
    ): Cursor? {
        return null
    }

    override fun getType(uri: Uri): String? {
        return null
    }

    override fun insert(uri: Uri, values: ContentValues?): Uri? {
        return null
    }

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int {
        return 0
    }

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int {
        return 0
    }

}