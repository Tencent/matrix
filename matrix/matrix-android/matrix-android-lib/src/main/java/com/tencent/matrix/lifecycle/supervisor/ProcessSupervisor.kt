package com.tencent.matrix.lifecycle.supervisor

import android.app.Application
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.ReduceOperators
import com.tencent.matrix.lifecycle.owners.CombinedProcessForegroundOwner
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil

/**
 * Created by Yves on 2021/9/26
 */
internal const val KEY_PROCESS_NAME = "KEY_PROCESS_NAME"

const val LRU_KILL_SUCCESS = 1
const val LRU_KILL_RESCUED = 2
const val LRU_KILL_CANCELED = 3
const val LRU_KILL_NOT_FOUND = 4

/**
 * supervisorProcess:
 * By default, we treats main process as supervisor.
 * If you want to specify ONE process as supervisor,
 * you could set this param with the full process name.
 *
 * for example:
 *  SupervisorConfig(application.packageName + ":push")
 *
 * Notice: pls avoid setting different names by different process.
 * Only ONE process can be chosen as supervisor, otherwise it would lead to crash
 *
 * BAD example:
 *  // code in [android.app.Application.onCreate] so each process would execute it
 *  override fun onCreate() {
 *      SupervisorConfig(getCurrentProcessName())
 *      //...
 *  }
 *
 * Created by Yves on 2021/10/22
 */
data class SupervisorConfig(val supervisorProcess: String = DEFAULT_PROCESS) {
    companion object {
        private const val DEFAULT_PROCESS = "main"
    }

    internal fun isTheChosenOne(application: Application): Boolean {
        return if (supervisorProcess == DEFAULT_PROCESS) {
            MatrixUtil.isInMainProcess(application)
        } else {
            MatrixUtil.getProcessName(application) == supervisorProcess
        }
    }
}


object ProcessSupervisor :
    MultiSourceStatefulOwner(ReduceOperators.OR) {

    internal val tag by lazy { "Matrix.Supervisor.Supervisor_${suffix()}" }

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

    internal val permission by lazy { "${application?.packageName}.matrix.permission.MEMORY_CANARY" }

    fun init(app: Application, config: SupervisorConfig, callback : (() -> Unit)? = null) {
        if (config.isTheChosenOne(app)) {
            initSupervisor(MatrixUtil.getProcessName(application), app)
            callback?.invoke()
        }
        inCharge(app)
    }

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

        CombinedProcessForegroundOwner.observeForever(object : IStateObserver {

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

    fun inCharge(app: Application, killedListener: () -> Boolean) {
        inCharge(app)
        DispatchReceiver.killedListener = killedListener
    }

    fun backgroundLruKill(killedCallback: (result: Int, process: String?) -> Unit) =
        SupervisorReceiver.backgroundLruKill(application, killedCallback)

    val isAppForeground: Boolean
        get() = active()

    val isSupervisor: Boolean
        get() = SupervisorReceiver.isSupervisor

    internal fun syncAppForeground() = turnOn()

    internal fun syncAppBackground() = turnOff()
}