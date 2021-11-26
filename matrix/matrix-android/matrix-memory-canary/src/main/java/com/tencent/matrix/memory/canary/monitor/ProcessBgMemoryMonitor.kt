package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.StagedBackgroundOwner
import com.tencent.matrix.memory.canary.MemInfo
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

/**
 * Created by Yves on 2021/11/3
 */

private const val TAG = "Matrix.monitor.BackgroundMemoryMonitor"

internal data class Threshold(
    private val size: Long,
    private val checkTimes: Int = 3,
) {
    private val check = arrayOf(0, 0)

    internal fun check(size: Long, staged: Boolean, cb: () -> Unit): Boolean =
        (size > this.size).also {
            if (staged) {
                if (it && check[0] < checkTimes && ++check[0] == checkTimes) {
                    cb.invoke()
                }
            } else {
                if (it && check[1] < checkTimes && ++check[1] == checkTimes) {
                    cb.invoke()
                }
            }
        }

    override fun toString(): String {
        return "{size = $size, checkTimes = ${checkTimes}}"
    }
}

class ProcessBgMemoryMonitorConfig(
    val enable: Boolean = true,
    val delayMillis: Long = TimeUnit.MINUTES.toMillis(1) + 500,
    javaThresholdByte: Long = 250 * 1024 * 1024L,
    nativeThresholdByte: Long = 500 * 1024 * 1024L,
    amsPssThresholdK: Long = 700 * 1024,
    debugPssThresholdK: Long = 700 * 1024,
    val reportCallback: (memInfo: MemInfo, staged: Boolean) -> Unit = { _, _ ->
        // do report
    },
) {
    private fun Long.asThreshold(checkTimes: Int = 3): Threshold {
        return Threshold(this, checkTimes)
    }

    internal val javaThresholdByte: Threshold = javaThresholdByte.asThreshold()
    internal val nativeThresholdByte: Threshold = nativeThresholdByte.asThreshold()
    internal val amsPssThresholdK: Threshold = amsPssThresholdK.asThreshold()
    internal val debugPssThresholdK: Threshold = debugPssThresholdK.asThreshold()

    override fun toString(): String {
        return "BackgroundMemoryMonitorConfig(delayMillis=$delayMillis, javaThresholdByte=$javaThresholdByte, nativeThresholdByte=$nativeThresholdByte, amsPssThresholdK=$amsPssThresholdK, debugPssThresholdK=$debugPssThresholdK)"
    }
}

class ProcessBgMemoryMonitor(private val config: ProcessBgMemoryMonitorConfig) {
    private val runningHandler = MatrixHandlerThread.getDefaultHandler()

    private val delayCheckTask = Runnable { checkWhenBackground() }

    fun init() {
        MatrixLog.i(TAG, "$config")
        if (!config.enable) {
            return
        }
        StagedBackgroundOwner.observeForever(object : IStateObserver {
            override fun on() { // foreground
                runningHandler.removeCallbacks(delayCheckTask)
            }

            override fun off() { // background
                runningHandler.postDelayed(delayCheckTask, config.delayMillis)
            }
        })
    }

    private var lastCheckTime = 0L

    private fun checkWhenBackground() {
        val current = System.currentTimeMillis()
        val lastCheckToNow = current - lastCheckTime
        lastCheckTime = current
        val memInfo = MemInfo.getCurrentProcessFullMemInfo()

        val staged = StagedBackgroundOwner.active()

        var shouldCallback = false

        val cb = {
            shouldCallback = true
        }

        val overThreshold = config.run {
            // @formatter:off
            arrayOf(
                "java" to javaThresholdByte.check(memInfo.javaMemInfo!!.usedByte, staged, cb),
                "native" to nativeThresholdByte.check(memInfo.nativeMemInfo!!.usedByte, staged, cb),
                "debugPss" to debugPssThresholdK.check(memInfo.debugPssInfo!!.totalPssK.toLong(), staged, cb),
                "amsPss" to (amsPssThresholdK.check(memInfo.amsPssInfo!!.totalPssK.toLong(), staged, cb) && lastCheckToNow > TimeUnit.MINUTES.toMillis(5))
            ).onEach { MatrixLog.i(TAG, "is over threshold ? $it") }.any { it.second }
            // @formatter:on
        }

        MatrixLog.i(
            TAG,
            "check: overThreshold: $overThreshold, is staged: $staged, interval: $lastCheckToNow, shouldCallback: $shouldCallback $memInfo"
        )

        if (overThreshold && shouldCallback) {
            MatrixLog.i(TAG, "report over threshold")
            config.reportCallback.invoke(memInfo, staged)
        }
    }

}