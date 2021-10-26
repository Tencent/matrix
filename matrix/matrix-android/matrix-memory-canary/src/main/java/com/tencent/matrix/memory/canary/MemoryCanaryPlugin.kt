package com.tencent.matrix.memory.canary

import com.tencent.matrix.memory.canary.lifecycle.owners.ActivityRecorder
import com.tencent.matrix.memory.canary.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

/**
 * supervisorProcess: you should parse a process name provided by [MatrixUtil.getProcessName]
 * Created by Yves on 2021/10/22
 */
class MemoryCanaryPlugin(private val supervisorProcess: String = DEFAULT_PROCESS) : Plugin() {

    companion object {
        private const val DEFAULT_PROCESS = "main"
    }

    private fun isTheChosenOne(): Boolean {
        return if (supervisorProcess == DEFAULT_PROCESS) {
            MatrixUtil.isInMainProcess(application)
        } else {
            MatrixUtil.getProcessName(application) == supervisorProcess
        }
    }

    override fun start() {
        if (status == PLUGIN_STARTED) {
            MatrixLog.e(tag, "already started")
            return
        }
        super.start()
        ActivityRecorder.init(application) // fixme move to [MultiProcessLifecycleInitializer]

        if (isTheChosenOne()) {
            ProcessSupervisor.initSupervisor(MatrixUtil.getProcessName(application), application)
        }
        ProcessSupervisor.inCharge(application)
    }

    override fun getTag(): String {
        return "MemoryCanary"
    }
}