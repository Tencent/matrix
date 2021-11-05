package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.ActivityRecorder
import com.tencent.matrix.lifecycle.owners.CombinedProcessForegroundOwner
import com.tencent.matrix.memory.canary.*
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
    private var busyCheck = 0
    private var freeCheck = 0

    internal fun check(size: Long, isBusy: Boolean, cb: () -> Unit): Boolean =
        (size > this.size).also {
            if (isBusy) {
                if (it && busyCheck < checkTimes && ++busyCheck == checkTimes) {
                    cb.invoke()
                }
            } else {
                if (it && freeCheck < checkTimes && ++freeCheck == checkTimes) {
                    cb.invoke()
                }
            }
        }

    override fun toString(): String {
        return "{size = $size, checkTimes = ${checkTimes}}"
    }
}

private fun Int.asThreshold(checkTimes: Int = 3): Threshold {
    return Threshold(this.toLong(), checkTimes)
}

private fun Long.asThreshold(checkTimes: Int = 3): Threshold {
    return Threshold(this, checkTimes)
}

class BackgroundMemoryMonitorConfig(
    val delayMillis: Long = TimeUnit.MINUTES.toMillis(1) + 500,
    javaThresholdByte: Long = 250 * 1024 * 1024L,
    nativeThresholdByte: Long = 500 * 1024 * 1024L,
    amsPssThresholdK: Long = 700 * 1024,
    debugPssThresholdK: Long = 700 * 1024,
    val baseActivities: Array<String> = emptyArray(),
    val reportCallback: (memInfo: MemInfo, isBusy: Boolean) -> Unit = { _, _ ->
        // do report
    },
) {
    internal val javaThresholdByte: Threshold = javaThresholdByte.asThreshold()
    internal val nativeThresholdByte: Threshold = nativeThresholdByte.asThreshold()
    internal val amsPssThresholdK: Threshold = amsPssThresholdK.asThreshold()
    internal val debugPssThresholdK: Threshold = debugPssThresholdK.asThreshold()

    override fun toString(): String {
        return "BackgroundMemoryMonitorConfig(delayMillis=$delayMillis, javaThresholdByte=$javaThresholdByte, nativeThresholdByte=$nativeThresholdByte, amsPssThresholdK=$amsPssThresholdK, debugPssThresholdK=$debugPssThresholdK)"
    }
}

class BackgroundMemoryMonitor(private val config: BackgroundMemoryMonitorConfig) {
    private val runningHandler = MatrixHandlerThread.getDefaultHandler()

    private val delayCheckTask = {
        checkWhenBackground()
    }

    fun init() {
        MatrixLog.i(TAG, "config memory threshold: $config")
        CombinedProcessForegroundOwner.observeForever(object : IStateObserver {
            override fun on() { // foreground
                runningHandler.removeCallbacks(delayCheckTask)
            }

            override fun off() { // background
                runningHandler.postDelayed(delayCheckTask, config.delayMillis)
            }
        })
    }

    private fun isBusy(): Boolean {
        return ActivityRecorder.active() && ActivityRecorder.sizeExcept(config.baseActivities) > 0
    }

    private var lastCheckTime = 0L

    private fun checkWhenBackground() {
        val current = System.currentTimeMillis()
        val lastCheckToNow = current - lastCheckTime
        lastCheckTime = current
        val memInfo = MemInfo.getCurrentProcessFullMemInfo()

        val busy = isBusy()

        var shouldCallback = false

        val cb = {
            shouldCallback = true
        }

        val overThreshold = config.run {
            // @formatter:off
            arrayOf(
                javaThresholdByte.check(memInfo.javaMemInfo!!.javaHeapUsedByte, busy, cb),
                nativeThresholdByte.check(memInfo.nativeMemInfo!!.nativeAllocatedByte, busy, cb),
                debugPssThresholdK.check(memInfo.debugPssInfo!!.totalPssK.toLong(), busy, cb),
                (amsPssThresholdK.check(memInfo.amsPssInfo!!.totalPssK.toLong(), busy, cb) && lastCheckToNow > TimeUnit.MINUTES.toMillis(5))
            ).any { it }
            // @formatter:on
        }

        MatrixLog.i(
            TAG,
            "check: is busy background: $busy, interval: $lastCheckToNow, overThreshold: $overThreshold, shouldCallback: $shouldCallback"
        )

        if (overThreshold) {
            MatrixLog.e(TAG, "memory over threshold: $memInfo")
        }

        if (overThreshold && shouldCallback) {
            MatrixLog.i(TAG, "report over threshold")
            config.reportCallback.invoke(memInfo, busy)
        }
    }

}