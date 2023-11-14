package com.tencent.matrix.lifecycle

import android.app.Application
import com.tencent.matrix.lifecycle.owners.*
import com.tencent.matrix.lifecycle.supervisor.*
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

// @formatter:off
object MatrixLifecycleLogger {

    private var application: Application? = null
    private val TAG by lazy { "Matrix.lifecycle.Logger_${String.format("%-10.10s", suffix())}" }

    private fun suffix(): String {
        return if (MatrixUtil.isInMainProcess(application!!)) {
            "Main"
        } else {
            val split = MatrixUtil.getProcessName(application!!).split(":").toTypedArray()
            if (split.size > 1) {
                split[1].takeLast(10)
            } else {
                "unknown"
            }
        }
    }

    fun init(app: Application, enable: Boolean) {
        application = app
        if (!enable) {
            MatrixLog.i(TAG, "logger disabled")
            return
        }

        ProcessUIResumedStateOwner.observeForever(object : ISerialObserver {
            override fun on() = MatrixLog.i(TAG, "ON_PROCESS_UI_RESUMED")
            override fun off() = MatrixLog.i(TAG, "ON_PROCESS_UI_PAUSED")
        })

        ProcessUIStartedStateOwner.observeForever(object : ISerialObserver {
            override fun on() = MatrixLog.i(TAG, "ON_PROCESS_UI_STARTED scene: ${ProcessUILifecycleOwner.recentScene}")
            override fun off() = MatrixLog.i(TAG, "ON_PROCESS_UI_STOPPED scene: ${ProcessUILifecycleOwner.recentScene}")
        })

        ProcessExplicitBackgroundOwner.observeForever(object : ISerialObserver {
            override fun on() = MatrixLog.i(TAG, "ON_PROCESS_ENTER_EXPLICIT_BACKGROUND")
            override fun off() = MatrixLog.i(TAG, "ON_PROCESS_EXIT_EXPLICIT_BACKGROUND")
        })

        ProcessStagedBackgroundOwner.observeForever(object : ISerialObserver {
            override fun on() = MatrixLog.i(TAG, "ON_PROCESS_ENTER_STAGED_BACKGROUND")
            override fun off() = MatrixLog.i(TAG, "ON_PROCESS_EXIT_STAGED_BACKGROUND")
        })

        ProcessDeepBackgroundOwner.observeForever(object : ISerialObserver {
            override fun on() = MatrixLog.i(TAG, "ON_PROCESS_ENTER_DEEP_BACKGROUND")
            override fun off() = MatrixLog.i(TAG, "ON_PROCESS_EXIT_DEEP_BACKGROUND")
        })

        AppUIForegroundOwner.observeForever(object : ISerialObserver {
            override fun on() = MatrixLog.i(TAG, "ON_APP_UI_ENTER_FOREGROUND scene: ${ProcessSupervisor.getRecentScene()}")
            override fun off() = MatrixLog.i(TAG, "ON_APP_UI_EXIT_FOREGROUND scene: ${ProcessSupervisor.getRecentScene()}")
        })

        AppExplicitBackgroundOwner.observeForever(object : ISerialObserver {
            override fun off() = MatrixLog.i(TAG, "ON_APP_EXIT_EXPLICIT_BACKGROUND")
            override fun on() = MatrixLog.i(TAG, "ON_APP_ENTER_EXPLICIT_BACKGROUND")
        })

        AppStagedBackgroundOwner.observeForever(object : ISerialObserver {
            override fun off() = MatrixLog.i(TAG, "ON_APP_EXIT_STAGED_BACKGROUND")
            override fun on() = MatrixLog.i(TAG, "ON_APP_ENTER_STAGED_BACKGROUND")
        })

        AppDeepBackgroundOwner.observeForever(object : ISerialObserver {
            override fun off() = MatrixLog.i(TAG, "ON_APP_EXIT_DEEP_BACKGROUND")
            override fun on() = MatrixLog.i(TAG, "ON_APP_ENTER_DEEP_BACKGROUND")
        })

        ProcessSupervisor.addDyingListener { scene, processName, pid ->
            MatrixLog.i(TAG, "Dying Listener: process $pid-$processName is dying on scene $scene")
            false // NOT rescue
        }

        ProcessSupervisor.addDeathListener { scene, processName, pid, isLruKill ->
            MatrixLog.i(
                TAG,
                "Death Listener: process $pid-$processName died on scene $scene, is LRU Kill? $isLruKill"
            )
        }

        ForegroundServiceLifecycleOwner.observeForever(object : ISerialObserver {
            override fun on() = MatrixLog.i(TAG, "ForegroundServiceLifecycleOwner: ON")
            override fun off() = MatrixLog.i(TAG, "ForegroundServiceLifecycleOwner: OFF")
        })

        OverlayWindowLifecycleOwner.observeForever(object : ISerialObserver {
            override fun on() = MatrixLog.i(TAG, "OverlayWindowLifecycleOwner: ON, hasOverlay = ${OverlayWindowLifecycleOwner.hasOverlayWindow()}, hasVisible = ${OverlayWindowLifecycleOwner.hasVisibleWindow()}")
            override fun off() = MatrixLog.i(TAG, "OverlayWindowLifecycleOwner: OFF, hasOverlay = ${OverlayWindowLifecycleOwner.hasOverlayWindow()}, hasVisible = ${OverlayWindowLifecycleOwner.hasVisibleWindow()}")
        })
    }
}