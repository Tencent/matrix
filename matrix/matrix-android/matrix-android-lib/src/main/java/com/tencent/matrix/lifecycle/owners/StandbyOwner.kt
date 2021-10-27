package com.tencent.matrix.lifecycle.owners

import android.os.Handler
import android.os.Looper
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

/**
 * State-ON:
 * EmptyBackground turned ON for more than 1min by default.
 *
 * Created by Yves on 2021/10/27
 */
object StandbyOwner : StatefulOwner(), IStateObserver {

    private const val TAG = "Matrix.memory.StandbyLifecycleOwner"

    private val DEFAULT_INTERVAL_MILLIS = TimeUnit.MINUTES.toMillis(1)

    private val mainHandler = Handler(Looper.getMainLooper())

    var intervalMillis = DEFAULT_INTERVAL_MILLIS

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