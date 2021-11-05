package com.tencent.matrix.memory.canary

import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.lifecycle.supervisor.SupervisorConfig
import com.tencent.matrix.memory.canary.monitor.BackgroundMemoryMonitor
import com.tencent.matrix.memory.canary.monitor.BackgroundMemoryMonitorConfig
import com.tencent.matrix.memory.canary.monitor.SumPssMonitor
import com.tencent.matrix.memory.canary.monitor.SumPssMonitorConfig
import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.util.MatrixLog

data class MemoryCanaryConfig(
    val supervisorConfig: SupervisorConfig = SupervisorConfig(),
    val sumPssMonitorConfig: SumPssMonitorConfig = SumPssMonitorConfig(),
    val backgroundMemoryMonitorConfig: BackgroundMemoryMonitorConfig = BackgroundMemoryMonitorConfig()
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
            if (ProcessSupervisor.init(application, supervisorConfig)) {
                sumPssMonitorConfig.takeIf { it.enable }?.let {
                    SumPssMonitor(it).start()
                }
            }
            backgroundMemoryMonitorConfig.takeIf { it.enable }?.let {
                BackgroundMemoryMonitor(it).init()
            }
        }
    }

    override fun getTag(): String {
        return "MemoryCanaryPlugin"
    }
}