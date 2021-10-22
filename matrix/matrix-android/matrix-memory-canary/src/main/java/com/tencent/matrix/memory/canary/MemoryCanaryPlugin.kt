package com.tencent.matrix.memory.canary

import android.app.Application
import com.tencent.matrix.memory.canary.lifecycle.owners.ActivityRecorder
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.plugin.Plugin
import com.tencent.matrix.plugin.PluginListener
import com.tencent.matrix.util.MatrixUtil

private typealias Initializer = (app: Application) -> Unit

/**
 * Created by Yves on 2021/10/22
 */
class MemoryCanaryPlugin(private val initializer: Initializer = defaultInitializer) : Plugin() {

    companion object {
        private val defaultInitializer: Initializer = { app ->
            ActivityRecorder.init(app)
            CombinedProcessForegroundStatefulOwner.also {
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

    override fun init(app: Application?, listener: PluginListener?) {
        super.init(app, listener)
        if (status == PLUGIN_INITED) {
            return
        }
        initializer.invoke(app!!)
    }

    override fun getTag(): String {
        return "MemoryCanary"
    }
}