package com.tencent.matrix.memory.canary

import android.app.Application
import com.tencent.matrix.memory.canary.lifecycle.owners.ActivityRecorder
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

private typealias Initializer = (app: Application) -> Unit

/**
 * Created by Yves on 2021/10/22
 */
class MemoryCanaryPlugin(private val initializer: Initializer = defaultInitializer) : Plugin() {

    companion object {
        private val defaultInitializer: Initializer = { app ->
            ActivityRecorder.init(app)
            CombinedProcessForegroundStatefulOwner.apply {
//                addSourceOwner(ForegroundServiceMonitor)
//                addSourceOwner(FloatingWindowMonitor)
//                addSourceOwner(AnyOtherForegroundWidgetMonitor)
            }

            if (MatrixUtil.isInMainProcess(app)) {
                ProcessSupervisor.initSupervisor(
                    MatrixUtil.getProcessName(app),
                    app
                )
            }
            ProcessSupervisor.inCharge(app)
        }
    }

    override fun start() {
        if (status == PLUGIN_STARTED) {
            MatrixLog.e(tag, "already started")
            return
        }
        super.start()
        initializer.invoke(application)
    }

    override fun getTag(): String {
        return "MemoryCanary"
    }
}