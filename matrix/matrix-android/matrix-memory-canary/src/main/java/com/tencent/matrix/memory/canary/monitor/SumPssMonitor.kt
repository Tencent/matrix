package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

private const val TAG = "Matrix.monitor.SumPssMonitor"

/**
 * Created by Yves on 2021/10/27
 */
data class SumPssMonitorConfig(
    val enable: Boolean = true,
    val intervalMillis: Long = TimeUnit.MINUTES.toMillis(5) + 1000,
    val thresholdKB: Long = 2 * 1024 * 1024L + 500 * 1024L, // 2.5G
    val checkTimes: Long = 3,
    val callback: (Int, Array<MemInfo>) -> Unit = { sumPssKB, memInfos ->
        MatrixLog.e(
            TAG,
            "sum pss of all process over threshold: $sumPssKB KB, detail: ${memInfos.contentToString()}"
        )
    }
)

class SumPssMonitor(
    private val config: SumPssMonitorConfig
) : TimerMonitor(config.intervalMillis) {

    private var overTimes = 0

    init {
        if (config.intervalMillis <= TimeUnit.MINUTES.toMillis(5)) {
            MatrixLog.w(TAG, "WARNING: pls consider to set the interval more than 5 minutes")
        }
    }

    override fun action() {
        MatrixLog.d(TAG, "check begin")

        val memInfos = MemInfo.getAllProcessPss()

        val sum = memInfos.onEach { info ->
            MatrixLog.i(
                TAG,
                "${info.processInfo?.pid}-${info.processInfo?.processName}: ${info.amsPssInfo!!.totalPss} KB"
            )
        }.sumBy { it.amsPssInfo!!.totalPss }.also { MatrixLog.i(TAG, "sumPss = $it KB") }

        MatrixLog.d(TAG, "check end sum = $sum")

        if (sum > config.thresholdKB) {
            overTimes++
        }
        if (overTimes >= config.checkTimes) {
            overTimes = 0
            config.callback(sum, memInfos)
        }
    }
}