package com.tencent.matrix.lifecycle.owners

import androidx.lifecycle.LifecycleOwner
import com.tencent.matrix.lifecycle.*
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

private val MAX_CHECK_INTERVAL = TimeUnit.MINUTES.toMillis(1)
private const val MAX_CHECK_TIMES = 20

/**
 * State-ON:
 * Activity is NOT in foreground
 * AND without foreground Service
 * AND without floating Views
 *
 * notice:
 *
 * ForegroundServiceLifecycleOwner is an optional StatefulOwner which is disabled by default and
 * can be enable by configuring [Matrix.Builder.enableFgServiceMonitor]
 *
 * Once this owner turned on, timer checker would be stop.
 * Therefore, the callback [IStateObserver.off] just means explicit background currently.

 * If the ForegroundServiceLifecycle were disabled and there are foreground Service launched
 * after calling [IStateObserver.off], the state wouldn't be turn on thus the [IStateObserver.on]
 * wouldn't be call either until we call the [active] or the state of upstream Owner changes.
 *
 * The state change event is delayed for at least 34ms for removing foreground widgets
 * like floating view which depends on [MatrixProcessLifecycleOwner]. see [TimerChecker]
 */
object ExplicitBackgroundOwner : StatefulOwner() {
    private const val TAG = "Matrix.background.Explicit"

    init {
        ImmutableMultiSourceStatefulOwner(
            ReduceOperators.OR,
            MatrixProcessLifecycleOwner.startedStateOwner,
            ForegroundServiceLifecycleOwner
        ).observeForever(object : IStateObserver {
            override fun on() { // Activity foreground
                checkTask.stop()
                turnOff()
            }

            override fun off() { // Activity background
                checkTask.post()
            }
        })
    }

    var maxCheckInterval = MAX_CHECK_INTERVAL
        set(value) {
            if (value < TimeUnit.SECONDS.toMillis(10)) {
                throw IllegalArgumentException("interval should NOT be less than 10s")
            }
            field = value
            MatrixLog.i(TAG, "set max check interval as $value")
        }

    private val checkTask = object : TimerChecker(TAG, maxCheckInterval) {
        override fun action(): Boolean {
            val uiForeground by lazy { MatrixProcessLifecycleOwner.startedStateOwner.active() }
            val fgService by lazy { MatrixProcessLifecycleOwner.hasForegroundService() }
            val visibleView by lazy { MatrixProcessLifecycleOwner.hasVisibleView() }

            if (uiForeground) {
                MatrixLog.i(TAG, "turn OFF for UI foreground")
                turnOff() // must be NOT in explicit background and do NOT need polling checker
                return false
            }

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

    var maxCheckInterval = MAX_CHECK_INTERVAL
        set(value) {
            if (value < TimeUnit.SECONDS.toMillis(10)) {
                throw IllegalArgumentException("interval should NOT be less than 10s")
            }
            field = value
            MatrixLog.i(TAG, "set max check interval as $value")
        }

    var maxCheckTimes = MAX_CHECK_TIMES
        set(value) {
            if (value <= 0) {
                throw IllegalArgumentException("max check times should be greater than 0")
            }
            field = value
            MatrixLog.i(TAG, "set max check interval as $value")
        }

    private val checkTask = object : TimerChecker(TAG, maxCheckInterval, maxCheckTimes) {
        override fun action(): Boolean {
            if (ExplicitBackgroundOwner.active()
                && (MatrixProcessLifecycleOwner.hasRunningAppTask()
                    .also { MatrixLog.i(TAG, "hasRunningAppTask? $it") }
                        || MatrixProcessLifecycleOwner.createdStateOwner.active())
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
        MatrixProcessLifecycleOwner.createdStateOwner.reverse(), // move to first to avoid useless checks
        ExplicitBackgroundOwner,
        StagedBackgroundOwner.reverse()
    )

    override fun active() = delegate.active()

    override fun observeForever(observer: IStateObserver) = delegate.observeForever(observer)

    override fun observeWithLifecycle(lifecycleOwner: LifecycleOwner, observer: IStateObserver) =
        delegate.observeWithLifecycle(lifecycleOwner, observer)

    override fun removeObserver(observer: IStateObserver) = delegate.removeObserver(observer)
}
