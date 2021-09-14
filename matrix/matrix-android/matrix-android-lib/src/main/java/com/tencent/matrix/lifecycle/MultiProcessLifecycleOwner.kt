package com.tencent.matrix.lifecycle

import android.app.Activity
import android.app.Application
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import androidx.lifecycle.*
import com.tencent.matrix.listeners.IAppForeground
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import java.util.*

internal const val TAG = "Matrix.MultiProcessLifecycle"

/**
 * multi process version of [androidx.lifecycle.ProcessLifecycleOwner]
 *
 * Created by Yves on 2021/9/14
 */
class MultiProcessLifecycleOwner : LifecycleOwner {

    companion object {
        private val sInstance = MultiProcessLifecycleOwner()

        private var sProcessName: String? = null

        private const val TIMEOUT_MS = 700L //mls

        @JvmStatic
        internal fun init(context: Context) {
            sProcessName = MatrixUtil.getProcessName(context)
            sInstance.attach(context)
            MatrixLog.i(TAG, "init for [$sProcessName]")
        }

        @JvmStatic
        fun get(): MultiProcessLifecycleOwner {
            return sInstance
        }
    }

    // ========================== base ========================== //

    private var mStartedCounter = 0
    private var mResumedCounter = 0

    private var mPauseSent = true
    private var mStopSent = true

    private val mMainHandler = Handler(Looper.getMainLooper())

    private val mRegistry = LifecycleRegistry(this)

    private val mDelayedPauseRunnable = Runnable {
        dispatchPauseIfNeeded()
        dispatchStopIfNeeded()
    }

    private fun activityStarted() {
        mStartedCounter++
        if (mStartedCounter == 1 && mStopSent) {
            mRegistry.handleLifecycleEvent(Lifecycle.Event.ON_START)
            mStopSent = false
        }
    }

    private fun activityResumed() {
        mResumedCounter++
        if (mResumedCounter == 1) {
            if (mPauseSent) {
                mRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
                mPauseSent = false
            } else {
                mMainHandler.removeCallbacks(mDelayedPauseRunnable)
            }
        }
    }

    private fun activityPaused() {
        mResumedCounter--
        if (mResumedCounter == 0) {
            mMainHandler.postDelayed(mDelayedPauseRunnable, TIMEOUT_MS)
        }
    }

    private fun activityStopped() {
        mStartedCounter--
        dispatchStopIfNeeded()
    }

    private fun dispatchPauseIfNeeded() {
        if (mResumedCounter == 0) {
            mPauseSent = true
            mRegistry.handleLifecycleEvent(Lifecycle.Event.ON_PAUSE)
        }
    }

    private fun dispatchStopIfNeeded() {
        if (mStartedCounter == 0 && mPauseSent) {
            mRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)
            mStopSent = true
        }
    }

    private fun attach(context: Context) {
        mMainHandler.postAtFrontOfQueue {
            mRegistry.addObserver(DefaultLifecycleObserver())
            mRegistry.handleLifecycleEvent(Lifecycle.Event.ON_CREATE)

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
    }

    override fun getLifecycle(): Lifecycle {
        return mRegistry
    }

    // ========================== extension ========================== //

    private val mListeners = HashSet<IAppForeground>()
    private val mRunningHandler = Handler(MatrixHandlerThread.getDefaultHandlerThread().looper)

    @get:JvmName("isAppForeground")
    var mAppForegrounded = false
    private set

    @get:JvmName("getVisibleScene")
    var mVisibleScene = "default"
    private set

    @set:JvmName("setCurrentFragmentName")
    @get:JvmName("getCurrentFragmentName")
    var mCurrentFragmentName: String? = null
        /**
         * must set after [Activity#onStart]
         */
        set(value) {
            MatrixLog.i(TAG, "[setCurrentFragmentName] fragmentName: $field")
            field = value
            value?.let {
                updateScene(it)
            } ?: run {
                updateScene("?")
            }
        }


    private var mPauseAsBgTimeoutMs = -1L

    fun setPauseAsBgIntervalMs(intervalMs: Long) {
        mPauseAsBgTimeoutMs = intervalMs
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
        mVisibleScene = activity.javaClass.name
    }

    private fun updateScene(scene: String) {
        mVisibleScene = scene
    }

    private fun onDispatchForeground(visibleScene: String) {
        if (mAppForegrounded) {
            return
        }
        MatrixLog.i(TAG, "onForeground... visibleScene[$visibleScene@$sProcessName]")
        mRunningHandler.post {
            mAppForegrounded = true
            synchronized(mListeners) {
                for (listener in mListeners) {
                    listener.onForeground(true)
                }
            }
        }
    }

    private fun onDispatchBackground(visibleScene: String) {
        if (!mAppForegrounded) {
            return
        }
        MatrixLog.i(TAG, "onBackground... visibleScene[$visibleScene@$sProcessName]")
        mRunningHandler.post {
            mAppForegrounded = false
            synchronized(mListeners) {
                for (listener in mListeners) {
                    listener.onForeground(false)
                }
            }
        }
    }

    private inner class DefaultLifecycleObserver : LifecycleObserver {
        private var mPauseConsumed = false

        private val mDelayedBgRunnable = Runnable {
            mPauseConsumed = true
            onDispatchBackground(mVisibleScene)
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_START)
        private fun onProcessStarted() {
            MatrixLog.i(TAG, "process [$sProcessName] onStarted")
            if (mPauseAsBgTimeoutMs < 0) {
                onDispatchForeground(mVisibleScene)
            }
            if (mPauseConsumed) {
                mPauseConsumed = false
                mRunningHandler.removeCallbacks(mDelayedBgRunnable)
            }
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
        private fun onProcessResumed() {
            MatrixLog.i(TAG, "process [$sProcessName] onResumed")
            if (mPauseAsBgTimeoutMs >= 0) {
                onDispatchForeground(mVisibleScene)
            }
            if (mPauseConsumed) {
                mPauseConsumed = false
                mRunningHandler.removeCallbacks(mDelayedBgRunnable)
            }
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
        private fun onProcessPaused() {
            MatrixLog.i(TAG, "process [$sProcessName] onPaused")
            if (mPauseAsBgTimeoutMs >= 0) {
                mRunningHandler.postDelayed(mDelayedBgRunnable, mPauseAsBgTimeoutMs)
            }
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
        private fun onProcessStopped() {
            MatrixLog.i(TAG, "process [$sProcessName] onStopped")
            if (mPauseConsumed && mPauseAsBgTimeoutMs >= 0) {
                mPauseConsumed = false
                MatrixLog.i(TAG, "process [$sProcessName] onStopped CANCELED")
                return
            }
            mRunningHandler.removeCallbacks(mDelayedBgRunnable)
            onDispatchBackground(mVisibleScene)
        }
    }
}