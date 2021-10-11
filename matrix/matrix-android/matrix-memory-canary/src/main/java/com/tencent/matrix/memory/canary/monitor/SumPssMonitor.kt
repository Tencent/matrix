package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.memory.canary.lifecycle.owners.ProcessSupervisor
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import java.util.concurrent.TimeUnit

/**
 * Created by Yves on 2021/9/28
 */
open class SumPssMonitor : Runnable {

    companion object {
        private const val TAG = "Matrix.monitor.SumPssMonitor"
        private const val DEFAULT_OVER_MEM_THRESHOLD = 2 * 1024 * 1024 // 2G
        private val CHECK_TIME = TimeUnit.MINUTES.toMillis(5)
    }

    private val handler = MatrixHandlerThread.getDefaultHandler()!!

    fun init() {
        handler.postDelayed(this, CHECK_TIME)
    }

    protected open fun reportSumPss(sumPss: Int) {
        if (sumPss > DEFAULT_OVER_MEM_THRESHOLD) {
            ProcessSupervisor.backgroundLruKill()
        }
    }

    override fun run() {
        MatrixLog.d(TAG, "check begin")

        val sum = MemoryInfoFactory.allProcessMemInfo.onEach { info ->
            MatrixLog.i(
                TAG,
                "${info.processInfo.pid}-${info.processInfo.processName}:ams-pss = ${info.amsPss}"
            )
        }.sumBy { it.amsPss }

        reportSumPss(sum)

        handler.postDelayed(this, CHECK_TIME)
    }

    interface OnOverloadCallback {
        fun overload(sumPss: Long)
    }
}

object SumPssReportMonitor : SumPssMonitor() {

    private const val TAG = "MicroMsg.monitor.SumPssMonitor"

    private var needReport = false

    init {
        needReport = true // TODO: 2021/10/9
    }

    override fun reportSumPss(sumPss: Int) {
        super.reportSumPss(sumPss)

        if (needReport) {
            MatrixLog.d(TAG, "report SumPss: $sumPss")
        }
    }
}