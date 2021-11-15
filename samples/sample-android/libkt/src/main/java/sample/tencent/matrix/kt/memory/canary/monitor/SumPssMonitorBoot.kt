package sample.tencent.matrix.kt.memory.canary.monitor

import com.tencent.matrix.memory.canary.MemInfo
import com.tencent.matrix.memory.canary.monitor.AppBgSumPssMonitorConfig
import com.tencent.matrix.util.MatrixLog

/**
 * Created by Yves on 2021/11/5
 */
object SumPssMonitorBoot {
    private const val TAG = "Matrix.sample.SumPssMonitorBoot"

    private val callback: (Int, Array<MemInfo>) -> Unit = { sumPssKB, memInfos ->
        MatrixLog.i(TAG, "report : $sumPssKB, $memInfos")
    }

    internal val config = AppBgSumPssMonitorConfig(callback = callback, thresholdKB = 2 * 1024 * 1024L)
}