package com.tencent.matrix.lifecycle.owners

import android.os.Handler
import android.os.Looper
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.ImmutableMultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.ReduceOperators
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

private const val TAG = "Matrix.EmptyBackgroundOwner"

/**
 * State-ON:
 * 1. Without any foreground widgets subscribed by [CombinedProcessForegroundOwner]
 * 2. Activity stack is empty
 *
 * Created by Yves on 2021/10/27
 */
object EmptyBackgroundOwner: ImmutableMultiSourceStatefulOwner(
    ReduceOperators.NONE,
    ActivityRecorder,
    CombinedProcessForegroundOwner
)

/**
 * State-ON:
 * EmptyBackground turned ON for more than 1min by default.
 *
 * Created by Yves on 2021/10/27
 */
object StandbyOwner : StatefulOwner(), IStateObserver {

    private val DEFAULT_INTERVAL_MILLIS = TimeUnit.MINUTES.toMillis(5)

    private val mainHandler = Handler(Looper.getMainLooper())

    var intervalMillis = DEFAULT_INTERVAL_MILLIS
    set(value) {
        field = value
        MatrixLog.i(TAG, "set interval as $value")
    }

    init {
        EmptyBackgroundOwner.observeForever(this)
    }

    private val countDownTask = Runnable {
        MatrixLog.d(TAG, "dispatch standby")
        turnOn()
    }

    private fun beginCountdown() {
        MatrixLog.d(TAG, "beginCountdown...")
        mainHandler.postDelayed(countDownTask, intervalMillis)
    }

    private fun cancelCountdown() {
        MatrixLog.d(TAG, "cancelCountdown")
        mainHandler.removeCallbacks(countDownTask)
        if (active()) {
            MatrixLog.d(TAG, "dispatch exit standby")
            turnOff()
        }
    }

    override fun on() {
        beginCountdown()
    }

    override fun off() {
        cancelCountdown()
    }
}