package com.tencent.matrix.memory.canary.monitor

/**
 * Created by Yves on 2021/12/9
 */

internal data class Threshold(
    private val size: Long,
    private val checkTimes: Int = 3,
    private val checkInterval: Long = 0,
) {
    private var check = 0
    private var lastCheckTime = 0L

    internal fun check(size: Long, cb: () -> Unit): Boolean {
        val current = System.currentTimeMillis()
        val lastCheckToNow = current - lastCheckTime
        lastCheckTime = current

        val over = size > this.size

        if (over && lastCheckToNow > checkInterval && check < checkTimes && ++check == checkTimes) {
            cb.invoke()
        } else if (!over && lastCheckToNow > checkInterval && check < checkTimes) {
            check = 0 // reset
        }

        return over && lastCheckToNow > checkInterval
    }

    override fun toString(): String {
        return "{size = $size, checkTimes = ${checkTimes}}"
    }
}

internal fun Long.asThreshold(checkTimes: Int = 3): Threshold {
    return Threshold(this, checkTimes)
}

internal fun Long.asThreshold(checkTimes: Int = 3, checkInterval: Long = 0): Threshold {
    return Threshold(this, checkTimes, checkInterval)
}