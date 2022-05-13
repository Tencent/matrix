package com.tencent.matrix.memory.canary.trim

import android.content.ComponentCallbacks2
import android.content.res.Configuration
import android.os.Handler
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent
import com.tencent.matrix.Matrix
import com.tencent.matrix.lifecycle.IBackgroundStatefulOwner
import com.tencent.matrix.lifecycle.IMatrixBackgroundCallback
import com.tencent.matrix.lifecycle.owners.ProcessDeepBackgroundOwner
import com.tencent.matrix.lifecycle.owners.ProcessStagedBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.AppDeepBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.AppStagedBackgroundOwner
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.safeApply
import java.util.concurrent.TimeUnit

interface TrimCallback {
    fun backgroundTrim()
    fun systemTrim(level: Int)
}

data class TrimMemoryConfig(
    val enable: Boolean = false,
    val delayMillis: ArrayList<Long> = arrayListOf(TimeUnit.MINUTES.toMillis(1))
)

/**
 * Trim memory when turned staged background or deep background for 1min or received system trim callback
 */
object TrimMemoryNotifier {

    private const val TAG = "Matrix.TrimMemoryNotifier"

    private val procTrimCallbacks = ArrayList<TrimCallback>()
    private val appTrimCallbacks = ArrayList<TrimCallback>()

    private fun ArrayList<TrimCallback>.backgroundTrim() {
        synchronized(this) {
            forEach {
                safeApply(TAG) {
                    it.backgroundTrim()
                }
            }
        }
        Runtime.getRuntime().gc()
    }

    private fun ArrayList<TrimCallback>.systemTrim(level: Int) {
        synchronized(this) {
            forEach {
                safeApply(TAG) {
                    it.systemTrim(level)
                }
            }
        }
        Runtime.getRuntime().gc()
    }

    class TrimTask(
        private val name: String,
        private val backgroundOwner: IBackgroundStatefulOwner,
        private val trimCallback: ArrayList<TrimCallback>,
        private val config: TrimMemoryConfig,
        private val immediate: Boolean
    ) : Runnable {

        private val runningHandler =
            Handler(MatrixHandlerThread.getDefaultHandlerThread().looper)

        @Volatile
        private var delayIndex = 0

        fun init() {
            backgroundOwner.addLifecycleCallback(object : IMatrixBackgroundCallback() {
                override fun onEnterBackground() {
                    delayIndex = 0
                    val delay = config.delayMillis[delayIndex]
                    runningHandler.removeCallbacksAndMessages(null)
                    if (immediate) {
                        trimCallback.backgroundTrim()
                        MatrixLog.i(TAG, "[$name] trim immediately")
                    }
                    runningHandler.postDelayed(this@TrimTask, delay)
                    MatrixLog.i(
                        TAG,
                        "[$name] trim delay[$delayIndex/${config.delayMillis.size}] $delay"
                    )
                }

                override fun onExitBackground() {
                    runningHandler.removeCallbacks(this@TrimTask)
                    delayIndex = 0
                }
            })
        }

        override fun run() {
            MatrixLog.i(
                TAG,
                "[$name] trim timeout [$delayIndex/${config.delayMillis.size}] ${config.delayMillis[delayIndex]}"
            )
            delayIndex++
            trimCallback.backgroundTrim()
            if (delayIndex < config.delayMillis.size) {
                val delay = config.delayMillis[delayIndex]
                runningHandler.postDelayed(this, delay)
                MatrixLog.i(
                    TAG,
                    "[$name] trim delay[$delayIndex/${config.delayMillis.size}] $delay"
                )
            }
        }
    }

    fun init(config: TrimMemoryConfig) {
        if (!config.enable) {
            return
        }

        if (config.delayMillis.isEmpty()) {
            throw IllegalArgumentException("config.delayMillis is empty")
        }

        if (!Matrix.isInstalled()) {
            MatrixLog.e(TAG, "Matrix NOT installed yet")
            return
        }

        // system trim
        Matrix.with().application.registerComponentCallbacks(object : ComponentCallbacks2 {
            override fun onLowMemory() {
                MatrixLog.e(TAG, "onLowMemory post")
                MatrixHandlerThread.getDefaultHandler().post {
                    MatrixLog.e(TAG, "onLowMemory")
                    procTrimCallbacks.systemTrim(ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL)
                    appTrimCallbacks.systemTrim(ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL)
                }
            }

            override fun onTrimMemory(level: Int) {
                if (level <= ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL) {
                    MatrixLog.e(TAG, "onTrimMemory post: $level")
                    MatrixHandlerThread.getDefaultHandler().post {
                        MatrixLog.e(TAG, "onTrimMemory: $level")
                        procTrimCallbacks.systemTrim(level)
                        appTrimCallbacks.systemTrim(level)
                    }
                }
            }

            override fun onConfigurationChanged(newConfig: Configuration) {}
        })

        // @formatter:off
        // process staged bg trim
        TrimTask("ProcessStagedBg", ProcessStagedBackgroundOwner, procTrimCallbacks, config, false).init()

        // process deep bg trim
        TrimTask("ProcessDeepBg", ProcessDeepBackgroundOwner, procTrimCallbacks, config, true).init()

        // app staged bg trim
        TrimTask("AppStagedBg", AppStagedBackgroundOwner, appTrimCallbacks, config, false).init()

        // app deep bg trim
        TrimTask("AppDeepBg", AppDeepBackgroundOwner, appTrimCallbacks, config, true).init()
        // @formatter:on
    }

    fun addProcessBackgroundTrimCallback(callback: TrimCallback) {
        synchronized(procTrimCallbacks) {
            procTrimCallbacks.add(callback)
        }
    }

    fun addProcessBackgroundTrimCallback(lifecycleOwner: LifecycleOwner, callback: TrimCallback) {
        synchronized(procTrimCallbacks) {
            procTrimCallbacks.add(callback)
        }
        lifecycleOwner.lifecycle.addObserver(object : LifecycleObserver {
            @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
            fun release() {
                synchronized(procTrimCallbacks) {
                    procTrimCallbacks.remove(callback)
                }
            }
        })
    }

    fun addAppBackgroundTrimCallback(callback: TrimCallback) {
        synchronized(appTrimCallbacks) {
            appTrimCallbacks.add(callback)
        }
    }

    fun addAppBackgroundTrimCallback(lifecycleOwner: LifecycleOwner, callback: TrimCallback) {
        synchronized(appTrimCallbacks) {
            appTrimCallbacks.add(callback)
        }
        lifecycleOwner.lifecycle.addObserver(object : LifecycleObserver {
            @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
            fun release() {
                synchronized(appTrimCallbacks) {
                    appTrimCallbacks.remove(callback)
                }
            }
        })
    }

    fun removeTrimCallback(callback: TrimCallback) {
        synchronized(procTrimCallbacks) {
            procTrimCallbacks.remove(callback)
        }
        synchronized(appTrimCallbacks) {
            appTrimCallbacks.remove(callback)
        }
    }
}