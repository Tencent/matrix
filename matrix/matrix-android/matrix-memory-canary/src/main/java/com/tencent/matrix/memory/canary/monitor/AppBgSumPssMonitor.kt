package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IBackgroundStatefulOwner
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.supervisor.AppStagedBackgroundOwner
import com.tencent.matrix.memory.canary.MemInfo
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

private const val TAG = "Matrix.monitor.AppBgSumPssMonitor"

/**
 * Created by Yves on 2021/10/27
 */
class AppBgSumPssMonitorConfig(
    val enable: Boolean = true,
    val bgStatefulOwner: IBackgroundStatefulOwner = AppStagedBackgroundOwner,
    val checkInterval: Long = TimeUnit.MINUTES.toMillis(3),
    thresholdKB: Long = 2 * 1024 * 1024L + 500 * 1024L, // 2.5G
    checkTimes: Int = 3,
    val callback: (sumPssKB: Int, Array<MemInfo>) -> Unit = { sumPssKB, memInfos ->
        MatrixLog.e(
            TAG,
            "sum pss of all process over threshold: $sumPssKB KB, detail: ${memInfos.contentToString()}"
        )
    }
) {
    internal val thresholdKB = thresholdKB.asThreshold(checkTimes, TimeUnit.MINUTES.toMillis(5))
    override fun toString(): String {
        return "AppBgSumPssMonitorConfig(enable=$enable, bgStatefulOwner=$bgStatefulOwner, checkInterval=$checkInterval, callback=${callback.javaClass.name}, thresholdKB=$thresholdKB)"
    }
}

internal class AppBgSumPssMonitor(
    private val config: AppBgSumPssMonitorConfig
) {

    private val checkTask = Runnable { check() }
    private val runningHandler = MatrixHandlerThread.getDefaultHandler()

    fun init() {
        MatrixLog.i(TAG, "$config")
        if (!config.enable) {
            return
        }
        config.bgStatefulOwner.observeForever(object : IStateObserver {
            override fun off() { // app foreground
                runningHandler.removeCallbacks(checkTask)
            }

            override fun on() { // app background
                runningHandler.postDelayed(checkTask, config.checkInterval)
            }
        })
    }

    private fun check() {
        val memInfos = MemInfo.getAllProcessPss()

        val sum = memInfos.onEach { info ->
            MatrixLog.i(
                TAG,
                "${info.processInfo?.pid}-${info.processInfo?.name}: ${info.amsPssInfo!!.totalPssK} KB"
            )
        }.sumBy { it.amsPssInfo!!.totalPssK }.also { MatrixLog.i(TAG, "sumPss = $it KB") }

        MatrixLog.i(TAG, "check end sum = $sum ${memInfos.contentToString()}")

        config.thresholdKB.check(sum.toLong()) {
            config.callback.invoke(sum, memInfos)
        }
    }
}