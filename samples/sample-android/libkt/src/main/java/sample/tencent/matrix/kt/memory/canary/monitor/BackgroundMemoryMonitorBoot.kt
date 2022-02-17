package sample.tencent.matrix.kt.memory.canary.monitor

import com.tencent.matrix.lifecycle.owners.DeepBackgroundOwner
import com.tencent.matrix.lifecycle.owners.StagedBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.memory.canary.MemInfo
import com.tencent.matrix.memory.canary.monitor.AppBgSumPssMonitorConfig
import com.tencent.matrix.memory.canary.monitor.ProcessBgMemoryMonitorConfig
import com.tencent.matrix.util.MatrixLog

/**
 * Created by Yves on 2021/11/5
 */
object BackgroundMemoryMonitorBoot {

    private const val TAG = "Matrix.sample.BackgroundMemoryMonitor"

    private val appBgMemCallback: (Int, Array<MemInfo>) -> Unit = { sumPssKB, memInfos ->
        MatrixLog.i(TAG, "report : $sumPssKB, ${memInfos.contentToString()}")
    }

    internal val appStagedBgMemoryMonitorConfig =
        AppBgSumPssMonitorConfig(
            callback = appBgMemCallback,
            bgStatefulOwner = ProcessSupervisor.appStagedBackgroundOwner,
            thresholdKB = 2 * 1024L,
            checkInterval = 10 * 1000L,
            checkTimes = 1,
        )
    internal val appDeepBgMemoryMonitorConfig =
        AppBgSumPssMonitorConfig(
            callback = appBgMemCallback,
            bgStatefulOwner = ProcessSupervisor.appDeepBackgroundOwner,
            thresholdKB = 2 * 1024L,
            checkInterval = 10 * 1000L
        )

    private val procBgMemCallback: (MemInfo) -> Unit = { memInfo ->
        MatrixLog.d(TAG, "report: $memInfo")
    }

    internal val procStagedBgMemoryMonitorConfig = ProcessBgMemoryMonitorConfig(
        reportCallback = procBgMemCallback,
        bgStatefulOwner = StagedBackgroundOwner,
        checkInterval = 20 * 1000L,
        javaThresholdByte = 10 * 1000L,
        nativeThresholdByte = 10 * 1024L,
        checkTimes = 1
    )

    internal val procDeepBgMemoryMonitorConfig = ProcessBgMemoryMonitorConfig(
        reportCallback = procBgMemCallback,
        bgStatefulOwner = DeepBackgroundOwner,
        checkInterval = 20 * 1000L,
        javaThresholdByte = 10 * 1000L,
        nativeThresholdByte = 10 * 1024L,
        checkTimes = 1
    )
}