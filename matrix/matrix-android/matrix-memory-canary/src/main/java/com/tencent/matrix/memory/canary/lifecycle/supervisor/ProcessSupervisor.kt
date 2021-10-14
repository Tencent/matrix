package com.tencent.matrix.memory.canary.lifecycle.supervisor

import android.app.Application
import com.tencent.matrix.memory.canary.lifecycle.IStateObserver
import com.tencent.matrix.memory.canary.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.memory.canary.lifecycle.ReduceOperators
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

/**
 * Created by Yves on 2021/9/26
 */
internal const val SUPERVISOR_PERMISSION = "com.tencent.matrix.permission.MEMORY_CANARY"
internal const val KEY_PROCESS_NAME = "KEY_PROCESS_NAME"

object ProcessSupervisor :
    MultiSourceStatefulOwner(ReduceOperators.OR) {

    internal val tag by lazy { "Matrix.memory.Supervisor_${suffix()}" }

    private fun suffix(): String {
        return if (MatrixUtil.isInMainProcess(application)) {
            "Main"
        } else {
            val split = MatrixUtil.getProcessName(application).split(":").toTypedArray()
            if (split.size > 1) {
                split[1]
            } else {
                "unknown"
            }
        }
    }

    private var application: Application? = null

    /**
     * should be call only once in process with the maximum life span
     */
    fun initSupervisor(process: String, app: Application) {
        application = app
        SupervisorReceiver.supervisorProcessName = MatrixUtil.getProcessName(application)
        if (process == SupervisorReceiver.supervisorProcessName) { // ur the chosen one
            SupervisorReceiver.install(application)

            observeForever(object : IStateObserver {

                override fun on() {
                    DispatchReceiver.dispatchAppForeground(application)
                }

                override fun off() {
                    DispatchReceiver.dispatchAppBackground(application)
                }
            })
        }
    }

    /**
     * call by all processes
     */
    fun inCharge(app: Application) {
        application = app

        DispatchReceiver.install(application)

        SupervisorReceiver.sendOnProcessInit(application)

        CombinedProcessForegroundStatefulOwner.observeForever(object : IStateObserver {

            override fun on() {
                MatrixLog.d(tag, "CombinedForegroundProcessLifecycle: ON")
                SupervisorReceiver.sendOnProcessForeground(application)
            }

            override fun off() {
                MatrixLog.d(tag, "CombinedForegroundProcessLifecycle: OFF")
                SupervisorReceiver.sendOnProcessBackground(application)
            }
        })

        // for Test
//        MultiProcessLifecycleOwner.get().lifecycle.addObserver(object : LifecycleObserver {
//            @OnLifecycleEvent(Lifecycle.Event.ON_START)
//            fun onCurrentProcessForeground() {
////                MatrixLog.d(tag, "onCurrentProcessForeground")
//                SupervisorReceiver.sendOnProcessForeground(application)
//            }
//
//            @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
//            fun onCurrentProcessBackground() {
////                MatrixLog.d(tag, "onCurrentProcessBackground")
//                SupervisorReceiver.sendOnProcessBackground(application)
//            }
//        })
    }

    fun backgroundLruKill(): Boolean {
        if (!isSupervisor) {
            return false
        }
        try {
            SupervisorReceiver.backgroundProcessLru?.last {
                it != MatrixUtil.getProcessName(application)
            }?.let {
                DispatchReceiver.dispatchKill(application, it)
                return true
            }
        } catch (ignore: NoSuchElementException) {
            return false
        }
        return false
    }

    val isAppForeground: Boolean
        get() = active()

    val isSupervisor: Boolean
        get() = SupervisorReceiver.isSupervisor

    internal fun syncAppForeground() {
        turnOn()
    }

    internal fun syncAppBackground() {
        turnOff()
    }
}