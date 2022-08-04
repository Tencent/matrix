package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IBackgroundStatefulOwner
import com.tencent.matrix.lifecycle.IMatrixBackgroundCallback
import com.tencent.matrix.lifecycle.supervisor.AppStagedBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MemInfo
import java.util.concurrent.TimeUnit

private const val TAG = "Matrix.monitor.AppBgSumPssMonitor"

/**
 * Created by Yves on 2021/10/27
 */
class AppBgSumPssMonitorConfig(
    val enable: Boolean = true,
    val bgStatefulOwner: IBackgroundStatefulOwner = AppStagedBackgroundOwner,
    val checkInterval: Long = TimeUnit.MINUTES.toMillis(3),
    amsPssThresholdKB: Long = 2 * 1024 * 1024L + 500 * 1024L, // 2.5G
    debugPssThresholdKB: Long = 2 * 1024 * 1024L + 500 * 1024L, // 2.5G
    checkTimes: Int = 3,
    val callback: (amsPssSumKB: Int, debugPssSumKB: Int, amsMemInfos: Array<MemInfo>, debugMemInfos: Array<MemInfo>) -> Unit = { amsSumPssKB, debugSumPssKB, amsMemInfos, debugMemInfos ->
        MatrixLog.e(
            TAG,
            "sum pss of all process over threshold: amsSumPss = $amsSumPssKB KB, debugSumPss = $debugSumPssKB KB " +
                    "amsMemDetail: ${amsMemInfos.contentToString()}" +
                    "\n==========\n" +
                    "debugMemDetail: ${debugMemInfos.contentToString()}"
        )
    }
) {
    internal val amsPssThresholdKB =
        amsPssThresholdKB.asThreshold(checkTimes, TimeUnit.MINUTES.toMillis(5))
    internal val debugPssThresholdKB = debugPssThresholdKB.asThreshold(checkTimes)

    //    internal val
    override fun toString(): String {
        return "AppBgSumPssMonitorConfig(enable=$enable, bgStatefulOwner=$bgStatefulOwner, checkInterval=$checkInterval, callback=${callback.javaClass.name}, thresholdKB=$amsPssThresholdKB)"
    }
}

internal class AppBgSumPssMonitor(
    private val checkInterval: Long,
    private val bgStatefulOwner: IBackgroundStatefulOwner,
    private val configs: Array<AppBgSumPssMonitorConfig>
) {
    companion object {
        fun init(configs: Array<AppBgSumPssMonitorConfig>) {
            configs.groupBy { cfg -> cfg.checkInterval }.forEach { e ->
                e.value.groupBy { it.bgStatefulOwner }.forEach { e2 ->
                    AppBgSumPssMonitor(e.key, e2.key, e2.value.toTypedArray()).start()
                }
            }
        }
    }

    private val checkTask = Runnable { check() }
    private val runningHandler = MatrixHandlerThread.getDefaultHandler()

    private fun start() {
        MatrixLog.i(TAG, configs.contentToString())
        if (configs.none { it.enable }) {
            MatrixLog.i(TAG, "none enabled")
            return
        }

        bgStatefulOwner.addLifecycleCallback(object : IMatrixBackgroundCallback() {
            override fun onEnterBackground() {
                runningHandler.postDelayed(checkTask, checkInterval)
            }

            override fun onExitBackground() {
                runningHandler.removeCallbacks(checkTask)
            }
        })
    }

    private fun check() {
        val amsMemInfos = MemInfo.getAllProcessPss()
        val debugMemInfos = ProcessSupervisor.getAllProcessMemInfo() ?: emptyArray()

        val amsPssSum = amsMemInfos.onEach { info ->
            MatrixLog.i(
                TAG,
                "${info.processInfo?.pid}-${info.processInfo?.name}: ${info.amsPssInfo!!.totalPssK} KB"
            )
        }.sumBy { it.amsPssInfo!!.totalPssK }.also { MatrixLog.i(TAG, "sumPss = $it KB") }

        val debugPssSum = debugMemInfos.onEach {
            MatrixLog.i(
                TAG,
                "${it.processInfo?.pid}-${it.processInfo?.name}: dbgPss = ${it.debugPssInfo!!.totalPssK} KB, amsPss = ${it.amsPssInfo!!.totalPssK} KB"
            )
        }.sumBy { it.debugPssInfo!!.totalPssK }.also { MatrixLog.i(TAG, "ipc sumDbgPss = $it KB") }

        MatrixLog.i(TAG, "check with interval [$checkInterval] amsPssSum = $amsPssSum KB, ${amsMemInfos.contentToString()}")
        MatrixLog.i(TAG, "check with interval [$checkInterval] debugPssSum = $debugPssSum KB, ${debugMemInfos.contentToString()}")

        configs.forEach { config ->
            var shouldCallback = false

            val overThreshold = config.run {
                // @formatter:off
                arrayOf("amsPss" to amsPssThresholdKB.check(amsPssSum.toLong()) { shouldCallback = true },
                    "debugPss" to debugPssThresholdKB.check(debugPssSum.toLong()) { shouldCallback = true }
                ).onEach { MatrixLog.i(TAG, "is over threshold? $it") }.any { it.second }
                // @formatter:on
            }

            if (overThreshold && shouldCallback) {
                MatrixLog.i(TAG, "report over threshold")
                config.callback.invoke(amsPssSum, debugPssSum, amsMemInfos, debugMemInfos)
            }
        }
    }
}