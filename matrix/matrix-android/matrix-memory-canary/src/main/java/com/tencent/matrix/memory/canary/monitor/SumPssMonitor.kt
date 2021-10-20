package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

/**
 * Created by Yves on 2021/9/28
 */
class SumPssMonitor(
    intervalMillis: Long,
    private val thresholdKB: Long,
    private val checkTimes: Long,
    private val callback: (sumPssKB: Int) -> Unit
) : TimerMonitor(intervalMillis) {

    companion object {
        private const val TAG = "Matrix.monitor.SumPssMonitor"
    }

    private var overTimes = 0

    init {
        if (intervalMillis <= TimeUnit.MINUTES.toMillis(5)) {
            MatrixLog.w(TAG, "WARNING: pls consider to set the interval more than 5 minutes")
        }
    }

    override fun action() {
        MatrixLog.d(TAG, "check begin")

        val sum = MemoryInfoFactory.allProcessMemInfo.onEach { info ->
            MatrixLog.i(
                TAG,
                "${info.processInfo.pid}-${info.processInfo.processName}:AMS-STAT = ${info.memoryStatsFromAms}"
            )
        }.sumBy { it.amsPss }.also {MatrixLog.i(TAG, "sumPss = $it KB")}

        reportSumPss(sum)
    }

    private fun reportSumPss(sumPss: Int) {
        if (sumPss > thresholdKB) {
            overTimes++
        }
        if (overTimes >= checkTimes) {
            overTimes = 0
            callback(sumPss)
        }
    }
}