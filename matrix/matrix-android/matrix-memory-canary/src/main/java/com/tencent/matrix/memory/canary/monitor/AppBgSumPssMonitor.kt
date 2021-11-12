package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.memory.canary.MemInfo
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

private const val TAG = "Matrix.monitor.SumPssMonitor"

/**
 * Created by Yves on 2021/10/27
 */
data class AppBgSumPssMonitorConfig(
    val enable: Boolean = true,
    val delayMillis: Long = TimeUnit.MINUTES.toMillis(1),
    val thresholdKB: Long = 2 * 1024 * 1024L + 500 * 1024L, // 2.5G
    val checkTimes: Long = 3,
    val callback: (Int, Array<MemInfo>) -> Unit = { sumPssKB, memInfos ->
        MatrixLog.e(
            TAG,
            "sum pss of all process over threshold: $sumPssKB KB, detail: ${memInfos.contentToString()}"
        )
    }
) {
    override fun toString(): String {
        return "AppBgSumPssMonitorConfig(enable=$enable, delayMillis=$delayMillis, thresholdKB=$thresholdKB, checkTimes=$checkTimes, callback=$callback)"
    }
}

class AppBgSumPssMonitor(
    private val config: AppBgSumPssMonitorConfig
) {

    private val runningHandler = MatrixHandlerThread.getDefaultHandler()

    private val delayCheckTask = Runnable {  }

    fun init() {
        MatrixLog.i(TAG, "$config")
        if (!config.enable) {
            return
        }
        ProcessSupervisor.observeForever(object : IStateObserver {
            override fun on() { // app foreground
                runningHandler.removeCallbacks(delayCheckTask)
            }

            override fun off() { // app background
                runningHandler.postDelayed(delayCheckTask, config.delayMillis)
            }
        })
    }

    fun action() {
        MatrixLog.d(TAG, "check begin")

        val memInfos = MemInfo.getAllProcessPss()

        val sum = memInfos.onEach { info ->
            MatrixLog.i(
                TAG,
                "${info.processInfo?.pid}-${info.processInfo?.name}: ${info.amsPssInfo!!.totalPssK} KB"
            )
        }.sumBy { it.amsPssInfo!!.totalPssK }.also { MatrixLog.i(TAG, "sumPss = $it KB") }

        MatrixLog.d(TAG, "check end sum = $sum")

//        if (sum > config.thresholdKB) {
//            overTimes++
//        }
//        if (overTimes >= config.checkTimes) {
//            overTimes = 0
//            config.callback(sum, memInfos)
//        }
    }
}