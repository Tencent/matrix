package com.tencent.matrix.lifecycle.owners

import androidx.lifecycle.LifecycleOwner
import com.tencent.matrix.lifecycle.*
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

private val MAX_CHECK_INTERVAL = TimeUnit.MINUTES.toMillis(1)
private const val MAX_CHECK_TIMES = 20

/**
 * Usage:
 *  recommended readable APIs:
 *      ProcessExplicitBackgroundOwner.isBackground()
 *      ProcessExplicitBackgroundOwner.addLifecycleCallback(object : IMatrixLifecycleCallback() {
 *           override fun onForeground() {}
 *           override fun onBackground() {}
 *      })
 *      // auto remove callback when lifecycle destroyed
 *      ProcessExplicitBackgroundOwner.addLifecycleCallback(lifecycleOwner, object : IMatrixLifecycleCallback() {
 *           override fun onForeground() {}
 *           override fun onBackground() {}
 *      })
 *
 *  the origin abstract APIs are also available:
 *      ProcessExplicitBackgroundOwner.active() // return true when state ON, in other words, it is explicit background now
 *      ProcessExplicitBackgroundOwner.observeForever(object : IStateObserver {
 *          override fun on() {} // entered the state, in other words, turned explicit background
 *          override fun off() {} // exit the state, in other words, turned foreground
 *      })
 *      // auto remove callback when lifecycle destroyed
 *      ProcessExplicitBackgroundOwner.observeForeverWithLifecycle(lifecycleOwner, object : IStateObserver {
 *          override fun on() {}
 *          override fun off() {}
 *      })
 *
 * State-ON:
 *  Activity is NOT in foreground
 *  AND without foreground Service
 *  AND without floating Views
 * then callback [IStateObserver.on] and [IMatrixLifecycleCallback.onBackground] would be called
 *
 * NOTICE:
 *
 * [ForegroundServiceLifecycleOwner] is an optional StatefulOwner which is disabled by default and
 * can be enabled by [MatrixLifecycleConfig.enableFgServiceMonitor]
 * [OverlayWindowLifecycleOwner] is similar, which can be enabled by [MatrixLifecycleConfig.enableOverlayWindowMonitor]
 *
 * If the [ForegroundServiceLifecycleOwner] were disabled, this owner would start the timer checker
 * to check the ForegroundService states. If there were foreground Service launched after UI turned
 * background, the callback [IStateObserver.off] or [IMatrixLifecycleCallback.onForeground]
 * wouldn't be call until we call the [active] or the state of upstream Owner changes.
 * So do [OverlayWindowLifecycleOwner].
 */
object ProcessExplicitBackgroundOwner : StatefulOwner(), IBackgroundStatefulOwner {
    private const val TAG = "Matrix.background.Explicit"

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
            val uiForeground by lazy { ProcessUIStartedStateOwner.active() }
            val fgService by lazy { ForegroundServiceLifecycleOwner.hasForegroundService() }
            val visibleWindow by lazy { OverlayWindowLifecycleOwner.hasVisibleWindow() }

            if (uiForeground) {
                MatrixLog.i(TAG, "turn OFF for UI foreground")
                turnOff() // must be NOT in explicit background and do NOT need polling checker
                return false
            }

            if (!fgService && !visibleWindow) {
                MatrixLog.i(TAG, "turn ON")
                turnOn()
                return false
            }
            MatrixLog.i(TAG, "turn OFF: fgService=$fgService, visibleView=$visibleWindow")
            turnOff()
            return true
        }
    }

    init {
        object : ImmutableMultiSourceStatefulOwner(
            ReduceOperators.OR,
            ProcessUIStartedStateOwner,
            ForegroundServiceLifecycleOwner,
            OverlayWindowLifecycleOwner
        ), ISerialObserver {}.observeForever(object : ISerialObserver {
            override fun on() { // Activity foreground
                checkTask.stop()
                turnOff()
            }

            override fun off() { // Activity background
                checkTask.post()
            }
        })
    }

    /**
     * It is possible to trigger a manual check by calling this method after calling
     * stopForeground/removeFloatingView for more accurate state callbacks.
     */
    override fun active(): Boolean {
        return if (ProcessUIStartedStateOwner.active()) {
            turnOff()
            false
        } else {
            checkTask.checkAndPostIfNeeded()
            super.active()
        }
    }
}


/**
 * Usage:
 *  similar to [ProcessExplicitBackgroundOwner]
 *
 * State-ON:
 *  Process is explicitly in background: [ProcessExplicitBackgroundOwner] isActive
 *  BUT there are tasks of current process in the recent screen
 *
 * notice: same as [ProcessExplicitBackgroundOwner]
 */
object ProcessStagedBackgroundOwner : StatefulOwner(), IBackgroundStatefulOwner {
    private const val TAG = "Matrix.background.Staged"

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
            if (ProcessExplicitBackgroundOwner.active()
                && (ProcessUILifecycleOwner.hasRunningAppTask()
                    .also { MatrixLog.i(TAG, "hasRunningAppTask? $it") }
                        || ProcessUICreatedStateOwner.active())
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

    init {
        ProcessExplicitBackgroundOwner.observeForever(object : ISerialObserver {
            override fun on() { // explicit background
                checkTask.post()
            }

            override fun off() { // foreground
                checkTask.stop()
                turnOff()
            }
        })
    }

    override fun active(): Boolean {
        return if (!ProcessExplicitBackgroundOwner.active()) {
            turnOff()
            false
        } else {
            checkTask.checkAndPostIfNeeded()
            super.active()
        }
    }
}

/**
 * Usage:
 *  similar to [ProcessExplicitBackgroundOwner]
 *
 * State-ON:
 *  Process is explicitly in background: [ProcessExplicitBackgroundOwner] isActive
 *  AND there is NO task of current process in the recent screen
 *
 * notice: same as [ProcessExplicitBackgroundOwner]
 */
object ProcessDeepBackgroundOwner : StatefulOwner(), IBackgroundStatefulOwner {

    private val delegate = object : ImmutableMultiSourceStatefulOwner(
        ReduceOperators.AND,
        ProcessUICreatedStateOwner.reverse(), // move to first to avoid useless checks
        ProcessExplicitBackgroundOwner,
        ProcessStagedBackgroundOwner.reverse()
    ), ISerialObserver {}

    override fun active() = delegate.active()

    override fun observeForever(observer: IStateObserver) = delegate.observeForever(observer)

    override fun observeWithLifecycle(lifecycleOwner: LifecycleOwner, observer: IStateObserver) =
        delegate.observeWithLifecycle(lifecycleOwner, observer)

    override fun removeObserver(observer: IStateObserver) = delegate.removeObserver(observer)
}
