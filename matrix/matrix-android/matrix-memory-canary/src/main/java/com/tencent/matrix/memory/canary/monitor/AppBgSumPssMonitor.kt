package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.MatrixProcessLifecycleOwner
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
    val checkInterval: Long = TimeUnit.MINUTES.toMillis(1),
    val thresholdKB: Long = 2 * 1024 * 1024L + 500 * 1024L, // 2.5G
    val thresholdIncStepKB: Long = 100 * 1024L, // 100M
    val checkTimes: Long = 3,
    val callback: (sumPssKB: Int, Int, Array<MemInfo>, Boolean) -> Unit = { sumPssKB, overTimes, memInfos, inaccurate ->
        MatrixLog.e(
            TAG,
            "sum pss of all process over threshold: $sumPssKB KB, over times: $overTimes, detail: ${memInfos.contentToString()}: inaccurate: $inaccurate"
        )
    }
) {
    override fun toString(): String {
        return "AppBgSumPssMonitorConfig(enable=$enable, delayMillis=$checkInterval, thresholdKB=$thresholdKB, checkTimes=$checkTimes, callback=$callback)"
    }
}

class AppBgSumPssMonitor(
    private val config: AppBgSumPssMonitorConfig
) : Runnable {

    private val runningHandler = MatrixHandlerThread.getDefaultHandler()
    @Volatile
    private var inaccurate = false
    private var currentThresholdKB = config.thresholdKB
    private var overTimes = 0

    override fun run() {
        if (MatrixProcessLifecycleOwner.hasVisibleView() || MatrixProcessLifecycleOwner.hasVisibleView()) {
            inaccurate = true
            runningHandler.postDelayed(this, config.checkInterval / 2)
            return
        }

        action()
    }

    fun init() {
        MatrixLog.i(TAG, "$config")
        if (!config.enable) {
            return
        }
        ProcessSupervisor.appUIForegroundOwner.observeForever(object : IStateObserver {
            override fun on() { // app foreground
                inaccurate = false
                runningHandler.removeCallbacks(this@AppBgSumPssMonitor)
            }

            override fun off() { // app background
                runningHandler.postDelayed(this@AppBgSumPssMonitor, config.checkInterval)
            }
        })
    }

    private fun action() {
        MatrixLog.d(TAG, "check begin")

        val memInfos = MemInfo.getAllProcessPss()

        val sum = memInfos.onEach { info ->
            MatrixLog.i(
                TAG,
                "${info.processInfo?.pid}-${info.processInfo?.name}: ${info.amsPssInfo!!.totalPssK} KB"
            )
        }.sumBy { it.amsPssInfo!!.totalPssK }.also { MatrixLog.i(TAG, "sumPss = $it KB") }

        MatrixLog.d(TAG, "check end sum = $sum")

        if (sum > currentThresholdKB) {
            currentThresholdKB = sum + config.thresholdIncStepKB
            config.callback.invoke(sum, ++overTimes, memInfos, inaccurate)
        }
    }
}