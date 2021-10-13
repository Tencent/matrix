package com.tencent.matrix.memory.canary

import android.app.Application
import com.tencent.matrix.memory.canary.lifecycle.owners.ActivityRecorder
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.supervisor.ProcessSupervisor
import com.tencent.matrix.util.MatrixUtil

/**
 * Created by Yves on 2021/9/28
 */
object DefaultMemoryCanaryInitializer {
    private const val TAG = "Matrix.DefaultMemoryCanaryInit"

    @Volatile
    private var inited: Boolean = false

    fun init(app: Application) {
        if (inited) {
            return
        }
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

        inited = true
    }
}