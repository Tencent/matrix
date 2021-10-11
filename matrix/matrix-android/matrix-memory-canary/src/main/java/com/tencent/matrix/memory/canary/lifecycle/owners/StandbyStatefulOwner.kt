package com.tencent.matrix.memory.canary.lifecycle.owners

import android.os.Handler
import android.os.Looper
import com.tencent.matrix.memory.canary.lifecycle.IStateObserver
import com.tencent.matrix.memory.canary.lifecycle.StatefulOwner
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

/**
 * 「待机」定义：进入深后台 1min
 * Created by Yves on 2021/9/27
 */
// TODO: 2021/10/11 move
object StandbyStatefulOwner : StatefulOwner(), IStateObserver {

    private const val TAG = "MicroMsg.lifecycle.StandbyLifecycleOwner"

    private val mainHandler = Handler(Looper.getMainLooper())

    init {
        DeepBackgroundStatefulOwner.observeForever(this)
    }

    private val countDownTask = Runnable {
        MatrixLog.d(TAG, "dispatch standby")
        turnOn()
    }

    private fun beginCountdown() {
        MatrixLog.d(TAG, "beginCountdown...")
        mainHandler.postDelayed(countDownTask, TimeUnit.MINUTES.toMillis(1))
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