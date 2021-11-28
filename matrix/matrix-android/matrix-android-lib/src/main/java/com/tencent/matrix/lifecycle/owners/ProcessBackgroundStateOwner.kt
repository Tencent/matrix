package com.tencent.matrix.lifecycle.owners

import android.app.ActivityManager
import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.lifecycle.LifecycleOwner
import com.tencent.matrix.Matrix
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.ImmutableMultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.ReduceOperators
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.lifecycle.owners.ExplicitBackgroundOwner.active
import com.tencent.matrix.util.ForegroundWidgetDetector
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import java.util.concurrent.TimeUnit

private val MAX_CHECK_INTERVAL = TimeUnit.MINUTES.toMillis(1)

data class BackgroundStateConfig(val checkInterval: Long = MAX_CHECK_INTERVAL) // TODO: 2021/11/24 configure

/**
 * State-ON:
 * Activity is NOT in foreground
 * AND without foreground Service
 * AND without floating Views
 *
 * notice: Once this owner turned on, timer checker would be stop.
 * Therefore, the callback [IStateObserver.off] just means explicit background currently.
 * If foreground Service were launched after calling [IStateObserver.off],
 * the state wouldn't be turn on thus the [IStateObserver.on] wouldn't be call either
 * until we call the [active] or the state of upstream Owner changes.
 *
 * The state change event is delayed for at least 89ms for removing foreground widgets
 * like floating view which depends on [MatrixProcessLifecycleOwner]. see [TimerChecker]
 */
object ExplicitBackgroundOwner : StatefulOwner() {
    private const val TAG = "Matrix.ExplicitBackgroundOwner"

    init {
        MatrixProcessLifecycleOwner.startedStateOwner.observeForever(object : IStateObserver {
            override fun on() { // Activity foreground
                checkTask.stop()
                turnOff()
            }

            override fun off() { // Activity background
                checkTask.post()
            }
        })
    }

    private val checkTask = object : TimerChecker(TAG, MAX_CHECK_INTERVAL) {
        override fun action(): Boolean {
            val fgService = ForegroundWidgetDetector.hasForegroundService()
            val floatingView = ForegroundWidgetDetector.hasVisibleView()
            if (!fgService && !floatingView) {
                MatrixLog.i(TAG, "turn ON")
                turnOn()
                return false
            }
            MatrixLog.i(TAG, "turn OFF: fgService=$fgService, floatingView=$floatingView")
            turnOff()
            return true
        }
    }

    /**
     * It is possible to trigger a manual check by calling this method after calling
     * stopForeground/removeFloatingView for more accurate state callbacks.
     */
    override fun active(): Boolean {
        return if (MatrixProcessLifecycleOwner.startedStateOwner.active()) {
            false
        } else {
            checkTask.checkAndPostIfNeeded()
            super.active()
        }
    }
}


/**
 * State-ON:
 * Process is explicitly in background: [ExplicitBackgroundOwner] isNotActive
 * BUT there are tasks in recent screen
 *
 * notice: same as [ExplicitBackgroundOwner]
 */
object StagedBackgroundOwner : StatefulOwner() {
    private const val TAG = "Matrix.StagedBackgroundStateOwner"

    @Volatile
    var isGap = false
        private set

    init {
        ExplicitBackgroundOwner.observeForever(object : IStateObserver {
            override fun on() { // explicit background
                checkTask.post()
            }

            override fun off() { // foreground
                checkTask.stop()
                turnOff()
            }
        })
    }

    private val checkTask = object : TimerChecker(TAG, MAX_CHECK_INTERVAL, 20) {
        override fun action(): Boolean {
            if (hasRunningAppTask().also { MatrixLog.i(TAG, "hasRunningAppTask? $it") }
                || MatrixProcessLifecycleOwner.createdStateOwner.active()) {
                MatrixLog.i(TAG, "turn ON")
                turnOn() // staged background
                return true
            }
            MatrixLog.i(TAG, "turn off")
            turnOff() // means deep background
            return false
        }
    }

    override fun active(): Boolean {
        return if (!ExplicitBackgroundOwner.active()) {
            false
        } else {
            checkTask.checkAndPostIfNeeded()
            super.active()
        }
    }

    private val packageName by lazy { MatrixUtil.getPackageName(Matrix.with().application) }
    private val processName by lazy { MatrixUtil.getProcessName(Matrix.with().application) }
    private val activityManager by lazy { Matrix.with().application.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager }
    private val componentToProcess by lazy { HashMap<String, String>() }
    private val activityInfoArray by lazy {
        Matrix.with().application.packageManager
            .getPackageInfo(
                packageName,
                PackageManager.GET_ACTIVITIES
            ).activities
    }

    private fun isCurrentProcessComponent(component: ComponentName?): Boolean {
        if (component == null) {
            return false
        }

        if (component.packageName != packageName) {
            return false
        }

        return processName == componentToProcess.getOrPut(component.className, {
            val info = activityInfoArray!!.find { it.name == component.className }
            if (info == null) {
                MatrixLog.e(TAG, "got task info not appeared in package manager $info")
                packageName
            } else {
                info.processName
            }
        })
    }

    private fun hasRunningAppTask(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            activityManager.appTasks
                .filter {
                    val i = isCurrentProcessComponent(it.taskInfo.baseIntent.component)
                    val o = isCurrentProcessComponent(it.taskInfo.origActivity)
                    val b = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                        isCurrentProcessComponent(it.taskInfo.baseActivity)
                    } else {
                        false
                    }
                    val t = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                        isCurrentProcessComponent(it.taskInfo.topActivity)
                    } else {
                        false
                    }

                    return@filter i || o || b || t
                }.onEach {
                    MatrixLog.i(TAG, "$processName task: ${it.taskInfo}")
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
                            it.taskInfo.id == -1 // // If it is not running, this will be -1
                        }
                    }
                }
        } else {
            false
        }
    }
}

object DeepBackgroundOwner : StatefulOwner() {
    private val delegate = ImmutableMultiSourceStatefulOwner(
        ReduceOperators.AND,
        ExplicitBackgroundOwner,
        ImmutableMultiSourceStatefulOwner(ReduceOperators.NONE, StagedBackgroundOwner)
    )

    override fun active() = delegate.active()

    override fun observeForever(observer: IStateObserver) = delegate.observeForever(observer)

    override fun observeWithLifecycle(lifecycleOwner: LifecycleOwner, observer: IStateObserver) =
        delegate.observeWithLifecycle(lifecycleOwner, observer)

    override fun removeObserver(observer: IStateObserver) = delegate.removeObserver(observer)
}
