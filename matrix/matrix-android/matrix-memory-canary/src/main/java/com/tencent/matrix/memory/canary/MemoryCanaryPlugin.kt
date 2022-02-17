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
import com.tencent.matrix.util.safeLet

@Suppress("ArrayInDataClass")
data class MemoryCanaryConfig(
    val appBgSumPssMonitorConfigs: Array<AppBgSumPssMonitorConfig> = arrayOf(
        AppBgSumPssMonitorConfig(bgStatefulOwner = ProcessSupervisor.appStagedBackgroundOwner),
        AppBgSumPssMonitorConfig(bgStatefulOwner = ProcessSupervisor.appDeepBackgroundOwner)
    ),
    val processBgMemoryMonitorConfigs: Array<ProcessBgMemoryMonitorConfig> = arrayOf(
        ProcessBgMemoryMonitorConfig(bgStatefulOwner = StagedBackgroundOwner),
        ProcessBgMemoryMonitorConfig(bgStatefulOwner = DeepBackgroundOwner)
    ),
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
            val isSupervisor = safeLet(tag, defVal = false) {
                ProcessSupervisor.isSupervisor // throws Exception when Supervisor disabled
            }
            if (isSupervisor) {
                MatrixLog.d(tag, "supervisor is ${MatrixUtil.getProcessName(application)}")

                appBgSumPssMonitorConfigs.forEach {
                    AppBgSumPssMonitor(it).init()
                }
            }
            processBgMemoryMonitorConfigs.forEach {
                ProcessBgMemoryMonitor(it).init()
            }
        }
    }

    override fun getTag(): String {
        return "Matrix.MemoryCanaryPlugin"
    }
}