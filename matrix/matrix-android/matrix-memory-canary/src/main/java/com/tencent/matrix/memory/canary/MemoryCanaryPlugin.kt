package com.tencent.matrix.memory.canary

import com.tencent.matrix.lifecycle.owners.DeepBackgroundOwner
import com.tencent.matrix.lifecycle.owners.StagedBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.memory.canary.monitor.AppBgSumPssMonitor
import com.tencent.matrix.memory.canary.monitor.AppBgSumPssMonitorConfig
import com.tencent.matrix.memory.canary.monitor.ProcessBgMemoryMonitor
import com.tencent.matrix.memory.canary.monitor.ProcessBgMemoryMonitorConfig
import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

@Suppress("ArrayInDataClass")
data class MemoryCanaryConfig(
    val appStagedBgSumPssMonitorConfig: AppBgSumPssMonitorConfig =
        AppBgSumPssMonitorConfig(bgStatefulOwner = ProcessSupervisor.appStagedBackgroundOwner),
    val appDeepBgSumPssMonitorConfig: AppBgSumPssMonitorConfig =
        AppBgSumPssMonitorConfig(bgStatefulOwner = ProcessSupervisor.appDeepBackgroundOwner),
    val procStagedBgMemoryMonitorConfig: ProcessBgMemoryMonitorConfig =
        ProcessBgMemoryMonitorConfig(bgStatefulOwner = StagedBackgroundOwner),
    val procDeepBgMemoryMonitorConfig: ProcessBgMemoryMonitorConfig =
        ProcessBgMemoryMonitorConfig(bgStatefulOwner = DeepBackgroundOwner),
    val baseActivities: Array<String> = emptyArray()
)

class MemoryCanaryPlugin(
    private val memoryCanaryConfig: MemoryCanaryConfig = MemoryCanaryConfig()
) : Plugin() {

    override fun start() {
        if (status == PLUGIN_STARTED) {
            MatrixLog.e(tag, "already started")
            return
        }
        super.start()

        memoryCanaryConfig.apply {
            if (ProcessSupervisor.isSupervisor) {
                MatrixLog.d(tag, "supervisor is ${MatrixUtil.getProcessName(application)}")
                AppBgSumPssMonitor(appStagedBgSumPssMonitorConfig).init()
                AppBgSumPssMonitor(appDeepBgSumPssMonitorConfig).init()
            }
            ProcessBgMemoryMonitor(procStagedBgMemoryMonitorConfig).init()
            ProcessBgMemoryMonitor(procDeepBgMemoryMonitorConfig).init()
        }
    }

    override fun getTag(): String {
        return "MemoryCanaryPlugin"
    }
}