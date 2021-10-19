package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.util.MatrixLog

/**
 * Created by Yves on 2021/9/28
 */
class SumPssMonitor(
    private val threshold: Long,
    private val checkTimes: Long,
    interval: Long,
    val callback: (sumPss: Int) -> Unit
) : TimerMonitor(interval) {

    companion object {
        private const val TAG = "Matrix.monitor.SumPssMonitor"
    }

    private var overTimes = 0

    override fun action() {
        MatrixLog.d(TAG, "check begin")

        val sum = MemoryInfoFactory.allProcessMemInfo.onEach { info ->
            MatrixLog.i(
                TAG,
                "${info.processInfo.pid}-${info.processInfo.processName}:AMS-STAT = ${info.memoryStatsFromAms}"
            )
        }.sumBy { it.amsPss }

        reportSumPss(sum)
    }

    private fun reportSumPss(sumPss: Int) {
        if (sumPss > threshold) {
            overTimes++
        }
        if (overTimes >= checkTimes) {
            overTimes = 0
            callback(sumPss)
        }
    }
}