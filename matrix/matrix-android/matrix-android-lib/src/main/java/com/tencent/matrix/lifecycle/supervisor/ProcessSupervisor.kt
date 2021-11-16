package com.tencent.matrix.lifecycle.supervisor

import android.app.Application
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.IBinder
import android.util.Log
import com.tencent.matrix.Matrix
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.MultiSourceStatefulOwner
import com.tencent.matrix.lifecycle.ReduceOperators
import com.tencent.matrix.lifecycle.owners.CombinedProcessForegroundOwner
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import com.tencent.matrix.util.safeApply

/**
 * Created by Yves on 2021/9/26
 */
internal const val KEY_PROCESS_NAME = "KEY_PROCESS_NAME"
internal const val KEY_PROCESS_PID = "KEY_PROCESS_PID"
internal const val KEY_PROCESS_BUSY = "KEY_PROCESS_BUSY"

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
    /**
     * If you specify an existing process as Supervisor but don't want to modify the boot order
     * pls set [autoCreate] to false and than it would be init manually by startService when
     * Matrix init within Supervisor process
     */
    val autoCreate: Boolean = false
)

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

    private var application: Application = Matrix.with().application

    val isAppForeground: Boolean
        get() = active()

    val isSupervisor: Boolean by lazy {
        application.packageManager.getPackageInfo(
            application.packageName,
            PackageManager.GET_SERVICES
        ).services.find {
            it.name == SupervisorService::class.java.name
        }?.let { it.processName == MatrixUtil.getProcessName(application) } ?: false
    }

    @Volatile
    internal var supervisorProxy: ISupervisorProxy? = null

    // FIXME: 2021/10/28
//    internal val permission by lazy { "${application?.packageName}.matrix.permission.PROCESS_SUPERVISOR" }
    internal val permission by lazy { "${application.packageName}.manual.dump" }

    fun init(config: SupervisorConfig): Boolean {
        if (isSupervisor) {
            initSupervisor()
        }
        inCharge(config.autoCreate)
        return isSupervisor
    }

    fun addKilledListener(listener: () -> Boolean) =
        DispatchReceiver.addKilledListener(listener)

    fun removeKilledListener(listener: () -> Boolean) =
        DispatchReceiver.removeKilledListener(listener)

    fun backgroundLruKill(killedResult: (result: Int, process: String?) -> Unit) {
        // TODO: 2021/11/12
    }

    private fun initSupervisor() {
        SupervisorService.start(application)
        MatrixLog.i(tag, "initSupervisor")
    }

    /**
     * call by all processes
     */
    private fun inCharge(autoCreate: Boolean) {

        val intent = Intent(application, SupervisorService::class.java)

        Log.i(tag, "bind to Supervisor") // todo report for test

        application.bindService(intent, object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
                MatrixLog.i(TAG, "on Supervisor Connected")

                supervisorProxy = ISupervisorProxy.Stub.asInterface(service)

                supervisorProxy?.apply {

                    safeApply(tag) { onProcessCreate(ProcessToken.current(application)) }

                    CombinedProcessForegroundOwner.observeForever(object : IStateObserver {

                        override fun on() {
                            MatrixLog.d(tag, "in charge process: foreground")
                            safeApply(tag) { onProcessForeground(ProcessToken.current(application)) }
                        }

                        override fun off() {
                            MatrixLog.d(tag, "in charge process: background")
                            safeApply(tag) { onProcessBackground(ProcessToken.current(application)) }
                        }
                    })
                }
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                MatrixLog.e(tag, "onServiceDisconnected $name")
            }
        }, if (autoCreate) Context.BIND_AUTO_CREATE else Context.BIND_ABOVE_CLIENT)

        DispatchReceiver.install(application)

        MatrixLog.i(tag, "inCharge")
    }

    internal fun syncAppForeground() = turnOn()

    internal fun syncAppBackground() = turnOff()
}