package com.tencent.matrix.memory.canary

import android.app.Application
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.owners.ProcessSupervisor
import com.tencent.matrix.memory.canary.monitor.SumPssReportMonitor
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

/**
 * Created by Yves on 2021/9/28
 */
open class DefaultMemoryCanaryInitializer(app: Application) {
    companion object {
        private const val TAG = "Matrix.DefaultMemoryCanaryInit"

        @Volatile
        private var inited: Boolean = false

        var application: Application? = null
            private set
    }

    init {
        application = app
        init(app)
    }

    fun init(app: Application) {
        if (inited) {
            return
        }
        application = app

        onInit(app)

        inited = true
    }

    protected open fun onInit(app: Application) {
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
            SumPssReportMonitor.init()
        }
        ProcessSupervisor.inCharge(app)

        MemoryCanaryTest.test()
//        MemoryCanaryTest.testLifecycle()
    }
}