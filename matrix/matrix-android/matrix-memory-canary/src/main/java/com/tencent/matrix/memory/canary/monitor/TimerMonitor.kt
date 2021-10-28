package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
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
        MatrixLog.i(TAG, "start ${javaClass.name}")
        handler.removeCallbacksAndMessages(null)
        handler.postDelayed(this, cycle)
    }

    fun stop() {
        MatrixLog.i(TAG, "stop ${javaClass.name}")
        handler.removeCallbacksAndMessages(null)
    }

    final override fun run() {
        action()

        handler.postDelayed(this, cycle)
    }

    abstract fun action()
}