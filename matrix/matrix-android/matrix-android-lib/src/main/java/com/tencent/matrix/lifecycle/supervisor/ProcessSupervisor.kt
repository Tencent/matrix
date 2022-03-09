package com.tencent.matrix.lifecycle.supervisor

import android.app.Application
import android.content.ComponentName
import android.content.Context.*
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.IBinder
import android.util.Log
import com.tencent.matrix.lifecycle.*
import com.tencent.matrix.lifecycle.owners.*
import com.tencent.matrix.util.*

/**
 * Created by Yves on 2021/9/26
 */
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
data class SupervisorConfig(
    val enable: Boolean = false,
    /**
     * If you specify an existing process as Supervisor but don't want to modify the boot order
     * pls set [autoCreate] to false and than it would be init manually by startService when
     * Matrix init in the Supervisor process
     */
    val autoCreate: Boolean = false,
    val lruKillerWhiteList: List<String> = emptyList()
)

// @formatter:off
object AppUIForegroundOwner: IStatefulOwner by ProcessSupervisor.appUIForegroundOwner
object AppExplicitBackgroundOwner: IBackgroundStatefulOwner by ProcessSupervisor.appExplicitBackgroundOwner
object AppDeepBackgroundOwner: IBackgroundStatefulOwner by ProcessSupervisor.appDeepBackgroundOwner
object AppStagedBackgroundOwner: IBackgroundStatefulOwner by ProcessSupervisor.appStagedBackgroundOwner
// @formatter:on

object ProcessSupervisor : IProcessListener by ProcessSubordinate.processListener {

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

    @Volatile
    private var application: Application? = null
    internal var config: SupervisorConfig? = null

    val isAppUIForeground: Boolean
        get() = appUIForegroundOwner.active()

    val isAppExplicitBackground: Boolean
        get() = appExplicitBackgroundOwner.active()

    val isAppStagedBackground: Boolean
        get() = appStagedBackgroundOwner.active()

    val isAppDeepBackground: Boolean
        get() = appDeepBackgroundOwner.active()

    val isSupervisor: Boolean by lazy {
        if (application == null) {
            throw IllegalStateException("Supervisor NOT initialized yet or Supervisor is disabled!!!")
        }

        val serviceInfo = safeLetOrNull(TAG) {
            application!!.packageManager.getPackageInfo(
                application!!.packageName, PackageManager.GET_SERVICES
            ).services.find {
                it.name == SupervisorService::class.java.name
            }
        }

        // serviceInfo might be null
        return@lazy MatrixUtil.getProcessName(application!!) == serviceInfo?.processName || SupervisorService.isSupervisor
    }

    @Volatile
    internal var supervisorProxy: ISupervisorProxy? = null

    internal const val STARTED_STATE_OWNER = "StartedStateOwner"
    internal const val EXPLICIT_BACKGROUND_OWNER = "ExplicitBackgroundOwner"
    internal const val DEEP_BACKGROUND_OWNER = "DeepBackgroundOwner"

    // @formatter:off
    internal val appUIForegroundOwner: StatefulOwner = DispatcherStateOwner(ReduceOperators.OR, ProcessUILifecycleOwner.startedStateOwner, STARTED_STATE_OWNER)
    internal val appExplicitBackgroundOwner: IBackgroundStatefulOwner = object : DispatcherStateOwner(ReduceOperators.AND, ProcessExplicitBackgroundOwner, EXPLICIT_BACKGROUND_OWNER), IBackgroundStatefulOwner {}
    internal val appDeepBackgroundOwner: IBackgroundStatefulOwner = object : DispatcherStateOwner(ReduceOperators.AND, ProcessDeepBackgroundOwner, DEEP_BACKGROUND_OWNER), IBackgroundStatefulOwner {}
    // @formatter:on

    private class AppStagedBackgroundOwner(
        private val delegate: MultiSourceStatefulOwner = object : MultiSourceStatefulOwner(
            ReduceOperators.AND,
            appExplicitBackgroundOwner.shadow(true),
            appDeepBackgroundOwner.reverse().shadow(true)
        ), ISerialObserver {}
    ) : IBackgroundStatefulOwner, IStatefulOwner by delegate

    internal val appStagedBackgroundOwner: IBackgroundStatefulOwner = AppStagedBackgroundOwner()

    fun init(app: Application, config: SupervisorConfig?): Boolean {
        if (config == null || !config.enable) {
            MatrixLog.i(TAG, "Supervisor is disabled")
            return false
        }
        this.config = config
        application = app
        if (isSupervisor) {
            initSupervisor(app)
        }
        inCharge(config.autoCreate, app)
        return isSupervisor
    }

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

        Log.i(tag, "bind to Supervisor")

        SupervisorPacemaker.install(app)

        val conn = object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
                MatrixLifecycleThread.handler.post { // do NOT run ipc in main thread
                    SupervisorPacemaker.uninstall()
                    SubordinatePacemaker.uninstall(app)
                    supervisorProxy = ISupervisorProxy.Stub.asInterface(service)
                    MatrixLog.i(tag, "on Supervisor Connected $supervisorProxy")

                    ProcessUILifecycleOwner.onSceneChangedListener =
                        object : ProcessUILifecycleOwner.OnSceneChangedListener {
                            override fun onSceneChanged(newScene: String, origin: String) {
                                MatrixLog.d(tag, "onSceneChanged: $origin -> $newScene")
                                supervisorProxy?.safeApply(tag) { onSceneChanged(newScene) }
                            }
                        }

                    supervisorProxy?.safeApply(tag, msg = "supervisor is $supervisorProxy") {
                        registerSubordinate(
                            DispatcherStateOwner.ownersToProcessTokens(app),
                            ProcessSubordinate.getSubordinate(app)
                        )
                    }
                    DispatcherStateOwner.attach(application!!)
                }
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                MatrixLifecycleThread.handler.post {
                    MatrixLog.e(tag, "onServiceDisconnected $name")
                    supervisorProxy = null
                    ProcessUILifecycleOwner.onSceneChangedListener = null
                    DispatcherStateOwner.detach()
                    SupervisorPacemaker.install(app)
                    // try to re-bind supervisor, but don't auto create here
                    safeApply(log = false) { app.unbindService(this) }

                    safeLet({
                        app.bindService(intent, this, BIND_ABOVE_CLIENT)
                        MatrixLog.e(tag, "rebound supervisor")
                    }, failed = {
                        // install subordinate pacemaker
                        MatrixLog.printErrStackTrace(tag, it, "rebound supervisor failed")
                        SubordinatePacemaker.install(app) {
                            safeApply(tag) {
                                app.bindService(intent, this, BIND_ABOVE_CLIENT)
                                SubordinatePacemaker.uninstall(app)
                                MatrixLog.i(tag, "subordinate pacemaker rebound supervisor")
                            }
                        }
                    })
                }
            }
        }

        app.bindService(
            intent,
            conn,
            if (autoCreate) (BIND_AUTO_CREATE.or(BIND_ABOVE_CLIENT)) else BIND_ABOVE_CLIENT
        )

        MatrixLog.i(tag, "inCharge")
    }
}