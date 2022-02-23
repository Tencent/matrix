package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.IBackgroundStatefulOwner
import com.tencent.matrix.lifecycle.owners.StagedBackgroundOwner
import com.tencent.matrix.memory.canary.MemInfo
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

/**
 * Created by Yves on 2021/11/3
 */

private const val TAG = "Matrix.monitor.BackgroundMemoryMonitor"

class ProcessBgMemoryMonitorConfig(
    val enable: Boolean = true,
    val bgStatefulOwner: IBackgroundStatefulOwner = StagedBackgroundOwner,
    val checkInterval: Long = TimeUnit.MINUTES.toMillis(3),
    javaThresholdByte: Long = 250 * 1024 * 1024L,
    nativeThresholdByte: Long = 500 * 1024 * 1024L,
    amsPssThresholdK: Long = 1024 * 1024,
    debugPssThresholdK: Long = 1024 * 1024,
    checkTimes: Int = 3,
    val callback: (memInfo: MemInfo) -> Unit = { _ ->
        // do report
    },
) {
    internal val javaThresholdByte: Threshold = javaThresholdByte.asThreshold(checkTimes)
    internal val nativeThresholdByte: Threshold = nativeThresholdByte.asThreshold(checkTimes)
    internal val debugPssThresholdK: Threshold = debugPssThresholdK.asThreshold(checkTimes)
    internal val amsPssThresholdK: Threshold =
        amsPssThresholdK.asThreshold(checkTimes, TimeUnit.MINUTES.toMillis(5))

    override fun toString(): String {
        return "ProcessBgMemoryMonitorConfig(enable=$enable, bgStatefulOwner=$bgStatefulOwner, checkInterval=$checkInterval, reportCallback=${callback.javaClass.name}, javaThresholdByte=$javaThresholdByte, nativeThresholdByte=$nativeThresholdByte, debugPssThresholdK=$debugPssThresholdK, amsPssThresholdK=$amsPssThresholdK)"
    }
}

internal class ProcessBgMemoryMonitor(private val config: ProcessBgMemoryMonitorConfig) {
    private val runningHandler = MatrixHandlerThread.getDefaultHandler()

    private val delayCheckTask = Runnable { check() }

    fun init() {
        MatrixLog.i(TAG, "$config")
        if (!config.enable) {
            return
        }
        config.bgStatefulOwner.observeForever(object : IStateObserver {
            override fun on() {
                runningHandler.postDelayed(delayCheckTask, config.checkInterval)
            }

            override fun off() {
                runningHandler.removeCallbacks(delayCheckTask)
            }
        })
    }

    private fun check() {

        val memInfo = MemInfo.getCurrentProcessFullMemInfo()
        var shouldCallback = false

        val overThreshold = config.run {
            // @formatter:off
            arrayOf(
                "java" to javaThresholdByte.check(memInfo.javaMemInfo!!.usedByte) { shouldCallback = true },
                "native" to nativeThresholdByte.check(memInfo.nativeMemInfo!!.usedByte) { shouldCallback = true },
                "debugPss" to debugPssThresholdK.check(memInfo.debugPssInfo!!.totalPssK.toLong()) { shouldCallback = true },
                "amsPss" to amsPssThresholdK.check(memInfo.amsPssInfo!!.totalPssK.toLong()) { shouldCallback = true }
            ).onEach { MatrixLog.i(TAG, "is over threshold ? $it") }.any { it.second }
            // @formatter:on
        }

        MatrixLog.i(
            TAG,
            "check: overThreshold: $overThreshold, shouldCallback: $shouldCallback $memInfo"
        )

        if (overThreshold && shouldCallback) {
            MatrixLog.i(TAG, "report over threshold")
            config.callback.invoke(memInfo)
        }
    }

}