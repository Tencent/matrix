package com.tencent.matrix.lifecycle.supervisor

import android.app.Application
import androidx.lifecycle.LifecycleOwner
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
data class SupervisorConfig(
    val supervisorProcess: String = DEFAULT_PROCESS,
) {
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

// FIXME: 2021/10/27 设置其他进程为 Supervisor，启动 foreground 回调会有延迟
object ProcessSupervisor : MultiSourceStatefulOwner(ReduceOperators.OR) {

    private const val TAG = "Matrix.ProcessSupervisor"

    internal val tag by lazy { "${TAG}_${suffix()}" }

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

    // FIXME: 2021/10/28
//    internal val permission by lazy { "${application?.packageName}.matrix.permission.PROCESS_SUPERVISOR" }
    internal val permission by lazy { "${application?.packageName}.manual.dump" }

    fun init(app: Application, config: SupervisorConfig): Boolean {
        if (config.isTheChosenOne(app)) {
            initSupervisor(MatrixUtil.getProcessName(application), app)
        }
        inCharge(app)
        return isSupervisor
    }

    fun addKilledListener(listener: () -> Boolean) =
        DispatchReceiver.addKilledListener(listener)

    fun removeKilledListener(listener: () -> Boolean) =
        DispatchReceiver.removeKilledListener(listener)

    fun backgroundLruKill(killedResult: (result: Int, process: String?) -> Unit) =
        SupervisorReceiver.backgroundLruKill(application, killedResult)

    val isAppForeground: Boolean
        get() = active()

    val isSupervisor: Boolean
        get() = SupervisorReceiver.isSupervisor

    private fun checkInstall() {
        if (application == null) {
            MatrixLog.w(TAG, "observer will NOT be notified util MemoryCanaryPlugin were started")
        }
    }

    override fun observeForever(observer: IStateObserver) {
        checkInstall()
        super.observeForever(observer)
    }

    override fun observeWithLifecycle(lifecycleOwner: LifecycleOwner, observer: IStateObserver) {
        checkInstall()
        super.observeWithLifecycle(lifecycleOwner, observer)
    }

    /**
     * should be call only once in process with the maximum life span
     */
    private fun initSupervisor(process: String, app: Application) {
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
    private fun inCharge(app: Application) {
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
    }

    internal fun syncAppForeground() = turnOn()

    internal fun syncAppBackground() = turnOff()
}