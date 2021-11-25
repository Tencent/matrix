package com.tencent.matrix.lifecycle.owners

import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import kotlin.math.min

/**
 * Created by Yves on 2021/11/24
 */
internal abstract class TimerChecker(private val tag: String, private val maxIntervalMillis: Long) {

    private val runningHandler by lazy { MatrixHandlerThread.getDefaultHandler() }

    /**
     * The initial delay interval is 12 + 31 = 34(ms)
     * Why 34? Removing foreground widgets like floating Views should be run in main thread
     * and depend on Activity background event, which is dispatched in Matrix handler thread.
     * And for most cases, the onDetach would be called within 10ms. It is just enough to
     * cover the cases
     *
     * valid intervals
     * 34,55,89,144,233,377,610,987,1597,2584,4181,6765,10946,17711,28657,46368,...,$max
     *
     */
    class IntervalFactory(private val maxVal: Long) {
        private val initialInterval = arrayOf(13L, 21L) // 10th and 11th Fibonacci sequence
        private var fibo = initialInterval.copyOf()

        fun next(): Long {
            return min((fibo[0] + fibo[1]).also { fibo[0] = fibo[1]; fibo[1] = it }, maxVal)
        }

        fun reset() {
            fibo = initialInterval.copyOf()
        }
    }

    private val intervalFactory by lazy { IntervalFactory(maxIntervalMillis) }

    private val task by lazy {
        object : Runnable {
            override fun run() {
                posted = true
                MatrixLog.i(tag, "run check task")
                if (!action()) {
                    postTimes = 0 // reset so that check task can be resume by checkAndPostIfNeeded
                } else if (postTimes++ < 20) {
                    val interval = intervalFactory.next()
                    MatrixLog.i(tag, "need recheck: next $interval")
                    runningHandler.postDelayed(this, interval)
                } else {
                    MatrixLog.i(tag, "paused polling check")
                }
            }
        }
    }

    /**
     * before the first task is scheduled, direct check should be ignored
     */
    @Volatile
    private var posted = false

    @Volatile
    private var postTimes = 0

    /**
     * return true if need recheck
     */
    protected abstract fun action(): Boolean

    fun checkAndPostIfNeeded(): Boolean {
        if (!posted) {
            MatrixLog.i(tag, "checkAndPostIfNeeded: not posted yet, during gap time")
            return false
        }
        MatrixLog.i(tag, "checkAndPostIfNeeded")
        runningHandler.removeCallbacks(task)
        if (action()) {
            runningHandler.postDelayed(task, intervalFactory.next())
            return true
        }
        return false
    }

    fun post() {
        intervalFactory.reset() // maybe we don't need reset here
        val interval = intervalFactory.next()
        MatrixLog.i(tag, "post check: $interval")
        runningHandler.removeCallbacks(task)
        runningHandler.postDelayed(task, interval)
    }

    fun stop() {
        posted = false
        postTimes = 0
        MatrixLog.i(tag, "stop")
        intervalFactory.reset()
        runningHandler.removeCallbacks(task)
    }
}