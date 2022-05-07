package sample.tencent.matrix.kt.lifecycle

import com.tencent.matrix.lifecycle.IMatrixBackgroundCallback
import com.tencent.matrix.lifecycle.IMatrixForegroundCallback
import com.tencent.matrix.lifecycle.IMatrixLifecycleCallback
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.*
import com.tencent.matrix.lifecycle.supervisor.*
import com.tencent.matrix.util.MatrixLog

// @formatter:off
object MatrixLifecycleLogger {
    private const val TAG = "Matrix.sample.LifecycleLogger"

    fun start() {

        /********************************* readable api ***************************************/
        ProcessUIResumedStateOwner.addLifecycleCallback(object : IMatrixForegroundCallback() {
            override fun onEnterForeground() = MatrixLog.i(TAG, "ON_PROCESS_UI_RESUMED")
            override fun onExitForeground() = MatrixLog.i(TAG, "ON_PROCESS_UI_PAUSED")
        })

        ProcessUIStartedStateOwner.addLifecycleCallback(object : IMatrixForegroundCallback() {
            override fun onEnterForeground() = MatrixLog.i(TAG, "ON_PROCESS_UI_STARTED scene: ${ProcessUILifecycleOwner.recentScene}")
            override fun onExitForeground() = MatrixLog.i(TAG, "ON_PROCESS_UI_STOPPED scene: ${ProcessUILifecycleOwner.recentScene}")
        })

        ProcessExplicitBackgroundOwner.addLifecycleCallback(object : IMatrixBackgroundCallback() {
            override fun onEnterBackground() = MatrixLog.i(TAG, "ON_PROCESS_ENTER_EXPLICIT_BACKGROUND")
            override fun onExitBackground() = MatrixLog.i(TAG, "ON_PROCESS_EXIT_EXPLICIT_BACKGROUND")
        })

        ProcessStagedBackgroundOwner.addLifecycleCallback(object : IMatrixBackgroundCallback() {
            override fun onEnterBackground() = MatrixLog.i(TAG, "ON_PROCESS_ENTER_STAGED_BACKGROUND")
            override fun onExitBackground() = MatrixLog.i(TAG, "ON_PROCESS_EXIT_STAGED_BACKGROUND")
        })

        ProcessDeepBackgroundOwner.addLifecycleCallback(object : IMatrixBackgroundCallback() {
            override fun onEnterBackground() = MatrixLog.i(TAG, "ON_PROCESS_ENTER_STAGED_BACKGROUND")
            override fun onExitBackground() = MatrixLog.i(TAG, "ON_PROCESS_EXIT_STAGED_BACKGROUND")
        })

        AppUIForegroundOwner.addLifecycleCallback(object : IMatrixForegroundCallback() {
            override fun onEnterForeground() = MatrixLog.i(TAG, "ON_APP_UI_ENTER_FOREGROUND scene: ${ProcessSupervisor.getRecentScene()}")
            override fun onExitForeground() = MatrixLog.i(TAG, "ON_APP_UI_EXIT_FOREGROUND scene: ${ProcessSupervisor.getRecentScene()}")
        })

        AppExplicitBackgroundOwner.addLifecycleCallback(object : IMatrixBackgroundCallback() {
            override fun onExitBackground() = MatrixLog.i(TAG, "ON_APP_EXIT_EXPLICIT_BACKGROUND")
            override fun onEnterBackground() = MatrixLog.i(TAG, "ON_APP_ENTER_EXPLICIT_BACKGROUND")
        })

        AppStagedBackgroundOwner.addLifecycleCallback(object : IMatrixBackgroundCallback() {
            override fun onExitBackground() = MatrixLog.i(TAG, "ON_APP_EXIT_STAGED_BACKGROUND")
            override fun onEnterBackground() = MatrixLog.i(TAG, "ON_APP_ENTER_STAGED_BACKGROUND")
        })

        AppDeepBackgroundOwner.addLifecycleCallback(object : IMatrixBackgroundCallback() {
            override fun onExitBackground() = MatrixLog.i(TAG, "ON_APP_EXIT_DEEP_BACKGROUND")
            override fun onEnterBackground() = MatrixLog.i(TAG, "ON_APP_ENTER_DEEP_BACKGROUND")
        })

        /********************************* abstract api ***************************************/

        ProcessUIResumedStateOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "ProcessUIResumedStateOwner: ON_RESUME -> ${ProcessUIResumedStateOwner.active()}")
            override fun off() = MatrixLog.i(TAG, "ProcessUIResumedStateOwner: ON_PAUSE -> ${ProcessUIResumedStateOwner.active()}")
        })

        ProcessUIStartedStateOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "ProcessUIStartedStateOwner: ON_START -> ${ProcessUIStartedStateOwner.active()}")
            override fun off() = MatrixLog.i(TAG, "ProcessUIStartedStateOwner: ON_STOP -> ${ProcessUIStartedStateOwner.active()}")
        })

        ProcessExplicitBackgroundOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "ProcessExplicitBackgroundOwner: ON -> ${ProcessExplicitBackgroundOwner.active()}")
            override fun off() = MatrixLog.i(TAG, "ProcessExplicitBackgroundOwner: OFF -> ${ProcessExplicitBackgroundOwner.active()}")
        })

        ProcessStagedBackgroundOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "ProcessStagedBackgroundOwner: ON -> ${ProcessStagedBackgroundOwner.active()}")
            override fun off() = MatrixLog.i(TAG, "ProcessStagedBackgroundOwner: OFF -> ${ProcessStagedBackgroundOwner.active()}")
        })

        ProcessDeepBackgroundOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "ProcessDeepBackgroundOwner: ON -> ${ProcessDeepBackgroundOwner.active()}")
            override fun off() = MatrixLog.i(TAG, "ProcessDeepBackgroundOwner: OFF -> ${ProcessDeepBackgroundOwner.active()}")
        })

        AppUIForegroundOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "AppUIForegroundOwner: ON -> ${AppUIForegroundOwner.active()} on scene: ${ProcessSupervisor.getRecentScene()}")
            override fun off() = MatrixLog.i(TAG, "AppUIForegroundOwner: OFF -> ${AppUIForegroundOwner.active()} on scene: ${ProcessSupervisor.getRecentScene()}")
        })

        AppExplicitBackgroundOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "AppExplicitBackgroundOwner: ON -> ${AppExplicitBackgroundOwner.active()}")
            override fun off() = MatrixLog.i(TAG, "AppExplicitBackgroundOwner: OFF -> ${AppExplicitBackgroundOwner.active()}")
        })

        AppStagedBackgroundOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "AppStagedBackgroundOwner: ON -> ${AppStagedBackgroundOwner.active()}")
            override fun off() = MatrixLog.i(TAG, "AppStagedBackgroundOwner: OFF -> ${AppStagedBackgroundOwner.active()}")
        })

        AppDeepBackgroundOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "AppDeepBackgroundOwner: ON -> ${AppDeepBackgroundOwner.active()}")
            override fun off() = MatrixLog.i(TAG, "AppDeepBackgroundOwner: OFF -> ${AppDeepBackgroundOwner.active()}")
        })

        ProcessSupervisor.addDyingListener { scene, processName, pid ->
            MatrixLog.i(TAG, "Dying Listener: process $pid-$processName is dying on scene $scene")
            false // NOT rescue
        }

        ProcessSupervisor.addDeathListener { scene, processName, pid, isLruKill ->
            MatrixLog.i(TAG, "Death Listener: process $pid-$processName died on scene $scene, is LRU Kill? $isLruKill")
        }

        ForegroundServiceLifecycleOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "ForegroundServiceLifecycleOwner: ON")
            override fun off() = MatrixLog.i(TAG, "ForegroundServiceLifecycleOwner: OFF")
        })

        OverlayWindowLifecycleOwner.observeForever(object : IStateObserver {
            override fun on() = MatrixLog.i(TAG, "OverlayWindowLifecycleOwner: ON, hasOverlay = ${OverlayWindowLifecycleOwner.hasOverlayWindow()}, hasVisible = ${OverlayWindowLifecycleOwner.hasVisibleWindow()}")
            override fun off() = MatrixLog.i(TAG, "OverlayWindowLifecycleOwner: OFF, hasOverlay = ${OverlayWindowLifecycleOwner.hasOverlayWindow()}, hasVisible = ${OverlayWindowLifecycleOwner.hasVisibleWindow()}")
        })
    }
}