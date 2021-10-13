package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.util.MatrixHandlerThread
import java.util.concurrent.TimeUnit

/**
 * Created by Yves on 2021/10/12
 */
abstract class TimerMonitor(private val cycle: Long = DEFAULT_CHECK_TIME) : Runnable {

    companion object {
        private const val TAG = "Matrix.monitor.TimerMonitor"
        private val DEFAULT_CHECK_TIME = TimeUnit.MINUTES.toMillis(5)
    }

    private val handler = MatrixHandlerThread.getDefaultHandler()!!

    fun start() {
        handler.removeCallbacksAndMessages(null)
        handler.postDelayed(this, cycle)
    }

    fun stop() {
        handler.removeCallbacksAndMessages(null)
    }

    final override fun run() {
        action()

        handler.postDelayed(this, cycle)
    }

    abstract fun action()
}