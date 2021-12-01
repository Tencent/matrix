package com.tencent.matrix.lifecycle.owners

import androidx.lifecycle.LifecycleOwner
import com.tencent.matrix.lifecycle.*
import com.tencent.matrix.lifecycle.owners.ExplicitBackgroundOwner.active
import com.tencent.matrix.util.MatrixLog
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
    private const val TAG = "Matrix.background.Explicit"

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
        // Foreground Service monitor is optional
        ForegroundServiceLifecycleOwner.observeForever(object : IStateObserver {
            override fun on() { // foreground service launched
                checkTask.stop()
                turnOff()
            }

            override fun off() { // foreground service stopped
                // only post check task when Activity turned background
                if (MatrixProcessLifecycleOwner.startedStateOwner.active()) {
                    return
                }
                checkTask.post()
            }
        })
    }

    private val checkTask = object : TimerChecker(TAG, MAX_CHECK_INTERVAL) {
        override fun action(): Boolean {
            val fgService by lazy { MatrixProcessLifecycleOwner.hasForegroundService() }
            val visibleView by lazy { MatrixProcessLifecycleOwner.hasVisibleView() }

            if (!fgService && !visibleView) {
                MatrixLog.i(TAG, "turn ON")
                turnOn()
                return false
            }
            MatrixLog.i(TAG, "turn OFF: fgService=$fgService, visibleView=$visibleView")
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
    private const val TAG = "Matrix.background.Staged"

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

    private val checkTask = object : TimerChecker(TAG, MAX_CHECK_INTERVAL, 25) {
        override fun action(): Boolean {
            if (MatrixProcessLifecycleOwner.hasRunningAppTask()
                    .also { MatrixLog.i(TAG, "hasRunningAppTask? $it") }
                || MatrixProcessLifecycleOwner.createdStateOwner.active()
            ) {
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
}

object DeepBackgroundOwner : StatefulOwner() {

    private const val TAG = "Matrix.background.Deep"

    private fun StatefulOwner.reverse(): IStatefulOwner = object : IStatefulOwner by this {
        override fun active() = !this@reverse.active()
            .also { MatrixLog.d(TAG, "${this@reverse.javaClass.name} is active? $it") }
    }

    private val delegate = ImmutableMultiSourceStatefulOwner(
        ReduceOperators.AND,
        ExplicitBackgroundOwner,
        StagedBackgroundOwner.reverse(),
        MatrixProcessLifecycleOwner.createdStateOwner.reverse()
    )

    override fun active() = delegate.active()

    override fun observeForever(observer: IStateObserver) = delegate.observeForever(observer)

    override fun observeWithLifecycle(lifecycleOwner: LifecycleOwner, observer: IStateObserver) =
        delegate.observeWithLifecycle(lifecycleOwner, observer)

    override fun removeObserver(observer: IStateObserver) = delegate.removeObserver(observer)
}
