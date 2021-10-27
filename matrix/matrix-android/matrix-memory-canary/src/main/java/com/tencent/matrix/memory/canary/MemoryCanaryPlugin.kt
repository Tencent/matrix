package com.tencent.matrix.memory.canary

import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

/**
 * supervisorProcess:
 * By default, MemoryCanaryPlugin treats main process as supervisor.
 * If you want to specify ONE process as supervisor,
 * you could set this param with the full process name.
 *
 * for example:
 *  MemoryCanaryPlugin(application.packageName + ":push")
 *
 * Notice: pls avoid setting different names by different process.
 * Only ONE process can be chosen as supervisor, otherwise it would lead to crash
 *
 * BAD example:
 *  // code in [android.app.Application.onCreate] so each process would execute it
 *  override fun onCreate() {
 *      MemoryCanaryPlugin(getCurrentProcessName())
 *  }
 *
 * Created by Yves on 2021/10/22
 */
class MemoryCanaryPlugin(private val supervisorProcess: String = DEFAULT_PROCESS) : Plugin() {
    // TODO do we need a config builder ?
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

        if (isTheChosenOne()) {
            ProcessSupervisor.initSupervisor(MatrixUtil.getProcessName(application), application)
        }
        ProcessSupervisor.inCharge(application)
    }

    override fun getTag(): String {
        return "MemoryCanary"
    }
}