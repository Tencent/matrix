package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IBackgroundStatefulOwner
import com.tencent.matrix.lifecycle.IMatrixBackgroundCallback
import com.tencent.matrix.lifecycle.supervisor.AppStagedBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.util.*
import java.util.concurrent.TimeUnit

private const val TAG = "Matrix.monitor.AppBgSumPssMonitor"

/**
 * Created by Yves on 2021/10/27
 */
class AppBgSumPssMonitorConfig(
    val enable: Boolean = true,
    val bgStatefulOwner: IBackgroundStatefulOwner = AppStagedBackgroundOwner,
    val checkInterval: Long = TimeUnit.MINUTES.toMillis(3),
    amsPssThresholdKB: Long = Long.MAX_VALUE,
    debugPssThresholdKB: Long = Long.MAX_VALUE,
    checkTimes: Int = 3,
    val callback: (amsPssSumKB: Int, debugPssSumKB: Int, amsMemInfos: Array<MemInfo>, debugMemInfos: Array<MemInfo>) -> Unit = { amsSumPssKB, debugSumPssKB, amsMemInfos, debugMemInfos ->
        MatrixLog.e(
            TAG,
            "sum pss of all process over threshold: amsSumPss = $amsSumPssKB KB, debugSumPss = $debugSumPssKB KB " +
                    "amsMemDetail: ${amsMemInfos.contentToString()}" +
                    "\n==========\n" +
                    "debugMemDetail: ${debugMemInfos.contentToString()}"
        )
    },
    val extraPssFactory: () -> Array<MemInfo> = { emptyArray() } /*{ arrayOf(MemInfo(processInfo = ProcessInfo(pid = -1, name = "extra_isolate", activity = "none"), amsPssInfo = PssInfo(totalPssK = 0)))}*/
) {
    internal val amsPssThresholdKB =
        amsPssThresholdKB.asThreshold(checkTimes, TimeUnit.MINUTES.toMillis(5))
    internal val debugPssThresholdKB = debugPssThresholdKB.asThreshold(checkTimes)

    //    internal val
    override fun toString(): String {
        return "AppBgSumPssMonitorConfig(enable=$enable, bgStatefulOwner=$bgStatefulOwner, checkInterval=$checkInterval, callback=${callback.javaClass.name}, thresholdKB=$amsPssThresholdKB, extraPssFactory=${extraPssFactory.javaClass.name})"
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

        val debugPssSum = safeLet(tag = TAG, defVal = 0) {
            debugMemInfos.filter { it.processInfo != null && it.debugPssInfo != null && it.amsPssInfo != null }.onEach {
                MatrixLog.i(
                    TAG,
                    "${it.processInfo?.pid}-${it.processInfo?.name}: dbgPss = ${it.debugPssInfo!!.totalPssK} KB, amsPss = ${it.amsPssInfo!!.totalPssK} KB"
                )
            }.sumBy { it.debugPssInfo!!.totalPssK }.also { MatrixLog.i(TAG, "ipc sumDbgPss = $it KB") }
        }

        MatrixLog.i(TAG, "check with interval [$checkInterval] amsPssSum = $amsPssSum KB, ${amsMemInfos.contentToString()}")
        MatrixLog.i(TAG, "check with interval [$checkInterval] debugPssSum = $debugPssSum KB, ${debugMemInfos.contentToString()}")

        configs.forEach { config ->
            var shouldCallback = false

            val extraPssInfo = config.extraPssFactory()
            val extraPssSum = extraPssInfo.onEach {
                MatrixLog.i(TAG,
                    "${it.processInfo!!.pid}-${it.processInfo!!.name}: extra total pss = ${it.amsPssInfo!!.totalPssK} KB"
                )
            }.sumBy { it.amsPssInfo!!.totalPssK }.also { if (extraPssInfo.isNotEmpty()) { MatrixLog.i(TAG, "extra sum pss = $it KB") } }

            val overThreshold = config.run {
                // @formatter:off
                arrayOf("amsPss" to amsPssThresholdKB.check((amsPssSum + extraPssSum).toLong()) { shouldCallback = true },
                    "debugPss" to debugPssThresholdKB.check((debugPssSum + extraPssSum).toLong()) { shouldCallback = true }
                ).onEach { MatrixLog.i(TAG, "is over threshold? $it") }.any { it.second }
                // @formatter:on
            }

            if (overThreshold && shouldCallback) {
                MatrixLog.i(TAG, "report over threshold")
                config.callback.invoke(amsPssSum + extraPssSum, debugPssSum + extraPssSum, amsMemInfos + extraPssInfo, debugMemInfos + extraPssInfo)
            }
        }
    }
}