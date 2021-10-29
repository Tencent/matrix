package com.tencent.matrix.lifecycle.owners

import android.annotation.SuppressLint
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
import com.tencent.matrix.listeners.IAppForeground
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import java.util.*

/**
 * multi process version of [androidx.lifecycle.ProcessLifecycleOwner]
 *
 * Created by Yves on 2021/9/14
 */
object MultiProcessLifecycleOwner : LifecycleOwner {

    private const val TAG = "Matrix.MultiProcessLifecycle"

    private var sProcessName: String? = null
    private const val TIMEOUT_MS = 500L //mls

    internal fun init(context: Context) {
        sProcessName = MatrixUtil.getProcessName(context)
        attach(context)
        MatrixLog.i(TAG, "init for [${sProcessName}]")
    }

    // ========================== base ========================== //

    private var mStartedCounter = 0
    private var mResumedCounter = 0

    private var mPauseSent = true
    private var mStopSent = true

    private val mRunningHandler = Handler(MatrixHandlerThread.getDefaultHandlerThread().looper)

    //    private val mMainHandler = Handler(Looper.getMainLooper())
    @SuppressLint("VisibleForTests")
    private val mRegistry = SafeLifecycleRegistry(this)

    private val mDelayedPauseRunnable = Runnable {
        dispatchPauseIfNeeded()
        dispatchStopIfNeeded()
    }

    private fun SafeLifecycleRegistry.handleLifecycleEventAsync(event: Lifecycle.Event) = mRunningHandler.post {
        this.handleLifecycleEvent(event)
    }

    private fun activityStarted() {
        mStartedCounter++
        if (mStartedCounter == 1 && mStopSent) {
            mRegistry.handleLifecycleEventAsync(Lifecycle.Event.ON_START)
            mStopSent = false
        }
    }

    private fun activityResumed() {
        mResumedCounter++
        if (mResumedCounter == 1) {
            if (mPauseSent) {
                mRegistry.handleLifecycleEventAsync(Lifecycle.Event.ON_RESUME)
                mPauseSent = false
            } else {
                mRunningHandler.removeCallbacks(mDelayedPauseRunnable)
            }
        }
    }

    private fun activityPaused() {
        mResumedCounter--
        if (mResumedCounter == 0) {
            mRunningHandler.postDelayed(mDelayedPauseRunnable, TIMEOUT_MS)
        }
        if (mResumedCounter < 0) {
            throw IllegalStateException("mResumedCounter = $mResumedCounter, you must init LifecycleOwner before starting any activity")
        }
    }

    private fun activityStopped() {
        mStartedCounter--
        dispatchStopIfNeeded()
    }

    private fun dispatchPauseIfNeeded() {
        if (mResumedCounter == 0) {
            mPauseSent = true
            mRegistry.handleLifecycleEventAsync(Lifecycle.Event.ON_PAUSE)
        }
    }

    private fun dispatchStopIfNeeded() {
        if (mStartedCounter == 0 && mPauseSent) {
            mRegistry.handleLifecycleEventAsync(Lifecycle.Event.ON_STOP)
            mStopSent = true
        }
        if (mStartedCounter < 0) {
            throw IllegalStateException("mStartedCounter = $mStartedCounter, you must init LifecycleOwner before starting any activity")
        }
    }

    private fun attach(context: Context) {
        mRegistry.addObserver(DefaultLifecycleObserver())
        mRegistry.handleLifecycleEventAsync(Lifecycle.Event.ON_CREATE)

        val app = context.applicationContext as Application
        app.registerActivityLifecycleCallbacks(object : Application.ActivityLifecycleCallbacks {

            override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
            }

            override fun onActivityStarted(activity: Activity) {
                updateScene(activity)
                activityStarted()
            }

            override fun onActivityResumed(activity: Activity) {
                activityResumed()
            }

            override fun onActivityPaused(activity: Activity) {
                activityPaused()
            }

            override fun onActivityStopped(activity: Activity) {
                activityStopped()
            }

            override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {
            }

            override fun onActivityDestroyed(activity: Activity) {
            }
        })
    }

    override fun getLifecycle() = mRegistry

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
            value?.let {
                updateScene(it)
            } ?: run {
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

    class DefaultLifecycleObserver : LifecycleObserver {
        @OnLifecycleEvent(Lifecycle.Event.ON_START)
        fun onDispatchForeground() {
            if (isProcessForeground) {
                return
            }
            MatrixLog.i(TAG, "onForeground... visibleScene[$visibleScene@$sProcessName]")
            mRunningHandler.post {
                isProcessForeground = true
                synchronized(mListeners) {
                    for (listener in mListeners) {
                        listener.onForeground(true)
                    }
                }
            }
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
        fun onDispatchBackground() {
            if (!isProcessForeground) {
                return
            }
            MatrixLog.i(TAG, "onBackground... visibleScene[$visibleScene@$sProcessName]")
            mRunningHandler.post {
                isProcessForeground = false
                synchronized(mListeners) {
                    for (listener in mListeners) {
                        listener.onForeground(false)
                    }
                }
            }
        }
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
        context?.let {
            init(it)
        } ?: run {
            throw IllegalStateException("context is null !!!")
        }
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