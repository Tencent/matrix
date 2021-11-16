package com.tencent.matrix.lifecycle.supervisor

import android.app.Application
import android.content.ComponentName
import android.content.Context.BIND_ABOVE_CLIENT
import android.content.Context.BIND_AUTO_CREATE
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.IBinder
import android.util.Log
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
 *
 * Disabled by default.
 *
 * usage: TODO
 *
 * Created by Yves on 2021/10/22
 */
@Deprecated("TODO: 2021/11/16 we're going to move the config to Manifest next version by configuring matrix extension")
data class SupervisorConfig(
    @Deprecated("")
    val enable: Boolean = false,
    /**
     * If you specify an existing process as Supervisor but don't want to modify the boot order
     * pls set [autoCreate] to false and than it would be init manually by startService when
     * Matrix init in the Supervisor process
     */
    @Deprecated("")
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

    private var application: Application? = null

    val isAppForeground: Boolean
        get() = active()

    val isSupervisor: Boolean by lazy {
        if (application == null) {
            throw IllegalStateException("Matrix NOT initialized yet !!!")
        }

        application!!.packageManager.getPackageInfo(
            application!!.packageName,
            PackageManager.GET_SERVICES
        ).services.find {
            it.name == SupervisorService::class.java.name
        }?.let {
            it.processName == MatrixUtil.getProcessName(application!!)
        } ?: false
    }

    @Volatile
    internal var supervisorProxy: ISupervisorProxy? = null

    // FIXME: 2021/10/28
//    internal val permission by lazy { "${application?.packageName}.matrix.permission.PROCESS_SUPERVISOR" }
    internal val permission by lazy { "${application?.packageName}.manual.dump" }

    fun init(app: Application, config: SupervisorConfig?): Boolean {
        if (config == null || !config.enable) {
            MatrixLog.i(TAG, "Supervisor is disabled")
            return false
        }
        application = app
        if (isSupervisor) {
            initSupervisor(app)
        }
        inCharge(config.autoCreate, app)
        return isSupervisor
    }

    fun addKilledListener(listener: (isCurrent: Boolean) -> Boolean) =
        DispatchReceiver.addKilledListener(listener)

    fun removeKilledListener(listener: (isCurrent: Boolean) -> Boolean) =
        DispatchReceiver.removeKilledListener(listener)

    fun backgroundLruKill(killedResult: (result: Int, process: String?, pid: Int) -> Unit) =
        SupervisorService.instance?.backgroundLruKill(killedResult)

    private fun initSupervisor(app: Application) {
        SupervisorService.start(app)
        MatrixLog.i(tag, "initSupervisor")
    }

    /**
     * call by all processes
     */
    private fun inCharge(autoCreate: Boolean, app: Application) {

        val intent = Intent(app, SupervisorService::class.java)

        Log.i(tag, "bind to Supervisor") // todo report for test

        app.bindService(intent, object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
                MatrixLog.i(TAG, "on Supervisor Connected")

                supervisorProxy = ISupervisorProxy.Stub.asInterface(service)

                supervisorProxy?.apply {

                    safeApply(tag) { onProcessCreate(ProcessToken.current(app)) }

                    CombinedProcessForegroundOwner.observeForever(object : IStateObserver {

                        override fun on() {
                            MatrixLog.d(tag, "in charge process: foreground")
                            safeApply(tag) { onProcessForeground(ProcessToken.current(app)) }
                        }

                        override fun off() {
                            MatrixLog.d(tag, "in charge process: background")
                            safeApply(tag) { onProcessBackground(ProcessToken.current(app)) }
                        }
                    })
                }
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                MatrixLog.e(tag, "onServiceDisconnected $name")
            }
        }, if (autoCreate) (BIND_AUTO_CREATE.or(BIND_ABOVE_CLIENT)) else BIND_ABOVE_CLIENT)

        MatrixLog.d(tag, "bind finish")

        DispatchReceiver.install(app)

        MatrixLog.i(tag, "inCharge")
    }

    internal fun syncAppForeground() = turnOn()

    internal fun syncAppBackground() = turnOff()
}