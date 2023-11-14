package com.tencent.matrix.memory.canary

import com.tencent.matrix.lifecycle.owners.ProcessDeepBackgroundOwner
import com.tencent.matrix.lifecycle.owners.ProcessStagedBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.AppDeepBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.AppStagedBackgroundOwner
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.memory.canary.monitor.AppBgSumPssMonitor
import com.tencent.matrix.memory.canary.monitor.AppBgSumPssMonitorConfig
import com.tencent.matrix.memory.canary.monitor.ProcessBgMemoryMonitor
import com.tencent.matrix.memory.canary.monitor.ProcessBgMemoryMonitorConfig
import com.tencent.matrix.memory.canary.trim.TrimMemoryConfig
import com.tencent.matrix.memory.canary.trim.TrimMemoryNotifier
import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import com.tencent.matrix.util.safeLet

@Suppress("ArrayInDataClass")
data class MemoryCanaryConfig(
    val appBgSumPssMonitorConfigs: Array<AppBgSumPssMonitorConfig> = arrayOf(
        AppBgSumPssMonitorConfig(bgStatefulOwner = AppStagedBackgroundOwner),
        AppBgSumPssMonitorConfig(bgStatefulOwner = AppDeepBackgroundOwner)
    ),
    val processBgMemoryMonitorConfigs: Array<ProcessBgMemoryMonitorConfig> = arrayOf(
        ProcessBgMemoryMonitorConfig(bgStatefulOwner = ProcessStagedBackgroundOwner),
        ProcessBgMemoryMonitorConfig(bgStatefulOwner = ProcessDeepBackgroundOwner)
    ),
    val trimMemoryConfig: TrimMemoryConfig = TrimMemoryConfig()
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

                AppBgSumPssMonitor.init(appBgSumPssMonitorConfigs)
            }
            processBgMemoryMonitorConfigs.forEach {
                ProcessBgMemoryMonitor(it).init()
            }
            TrimMemoryNotifier.init(trimMemoryConfig)
        }
    }

    override fun getTag(): String {
        return "Matrix.MemoryCanaryPlugin"
    }
}