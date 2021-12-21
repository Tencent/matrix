package com.tencent.matrix.memory.canary.trim

import android.content.ComponentCallbacks2
import android.content.res.Configuration
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent
import com.tencent.matrix.Matrix
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.DeepBackgroundOwner
import com.tencent.matrix.lifecycle.owners.StagedBackgroundOwner
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.safeApply
import java.util.concurrent.TimeUnit

interface TrimCallback {
    fun trim()
}

data class TrimMemoryConfig(val delayMillis: Long = TimeUnit.MINUTES.toMillis(1))

/**
 * Trim memory when turned staged background or deep background for 1min or received system trim callback
 */
object TrimMemoryNotifier {

    private const val TAG = "Matrix.TrimMemoryNotifier"

    private val trimCallbacks = ArrayList<TrimCallback>()

    private fun ArrayList<TrimCallback>.trim() {
        forEach {
            safeApply(TAG) {
                it.trim()
            }
        }
    }

    var config = TrimMemoryConfig()

    init {
        if (!Matrix.isInstalled()) {
            MatrixLog.e(TAG, "Matrix NOT installed yet")
        } else {
            Matrix.with().application.registerComponentCallbacks(object : ComponentCallbacks2 {
                override fun onLowMemory() {
                    MatrixLog.e(TAG, "onLowMemory")
                    trimCallbacks.trim()
                }

                override fun onTrimMemory(level: Int) {
                    if (level <= 15) {
                        MatrixLog.e(TAG, "onTrimMemory: $level")
                        trimCallbacks.trim()
                    }
                }

                override fun onConfigurationChanged(newConfig: Configuration) {}
            })


            val task = Runnable {
                trimCallbacks.trim()
            }

            val bgObserver = object : IStateObserver {
                val runningHandler = MatrixHandlerThread.getDefaultHandler()

                override fun on() {
                    runningHandler.removeCallbacksAndMessages(null)
                    runningHandler.postDelayed(task, config.delayMillis)
                }

                override fun off() {
                    runningHandler.removeCallbacks(task)
                }
            }

            bgObserver.let {
                StagedBackgroundOwner.observeForever(it)
                DeepBackgroundOwner.observeForever(it)
            }
        }
    }

    fun addTrimCallback(callback: TrimCallback) {
        trimCallbacks.add(callback)
    }

    fun addTrimCallback(lifecycleOwner: LifecycleOwner, callback: TrimCallback) {
        trimCallbacks.add(callback)
        lifecycleOwner.lifecycle.addObserver(object : LifecycleObserver {
            @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
            fun release() {
                trimCallbacks.remove(callback)
            }
        })
    }

    fun removeTrimCallback(callback: TrimCallback) {
        trimCallbacks.remove(callback)
    }
}