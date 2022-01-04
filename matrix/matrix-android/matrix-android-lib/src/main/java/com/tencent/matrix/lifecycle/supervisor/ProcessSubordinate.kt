package com.tencent.matrix.lifecycle.supervisor

import android.app.Application
import android.os.Build
import android.os.DeadObjectException
import android.os.Process
import android.text.TextUtils
import com.tencent.matrix.lifecycle.owners.OverlayWindowLifecycleOwner
import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner
import com.tencent.matrix.util.*
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit

internal interface IProcessListener {
    fun addDyingListener(listener: (recentScene: String?,processName: String?, pid: Int?) -> Boolean)
    fun removeDyingListener(listener: (recentScene: String?,processName: String?, pid: Int?) -> Boolean)
    fun addDeathListener(listener: (recentScene: String?, processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit)
    fun removeDeathListener(listener: (recentScene: String?, processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit)
    fun getRecentScene(): String
}

/**
 * Created by Yves on 2021/12/30
 */
internal object ProcessSubordinate {

    private val TAG by lazy { "${ProcessSupervisor.tag}.Subordinate" }

    internal val manager by lazy {
        if (ProcessSupervisor.isSupervisor) {
            Manager()
        } else {
            throw IllegalAccessException("NOT allow for subordinate processes")
        }
    }

    internal class Manager {
        private val subordinateProxies by lazy(LazyThreadSafetyMode.SYNCHRONIZED) { ConcurrentHashMap<ProcessToken, ISubordinateProxy>() }

        private fun ConcurrentHashMap<ProcessToken, ISubordinateProxy>.forEachSafe(action: (Map.Entry<ProcessToken, ISubordinateProxy>) -> Unit) {
            forEach { e ->
                safeLet(unsafe = { action(e) }, failed = {
                    MatrixLog.printErrStackTrace(TAG, it, "${e.key.pid}${e.key.name}")
                    if (it is DeadObjectException) {
                        MatrixLog.e(TAG, "remote process of proxy is dead, remove proxy: ${e.key}")
                        subordinateProxies.remove(e.key)
                    }
                })
            }
        }


        fun addProxy(process: ProcessToken, subordinate: ISubordinateProxy) =
            subordinateProxies.put(process, subordinate)

        fun removeProxy(process: ProcessToken) =
            subordinateProxies.remove(process)


        fun dispatchState(scene: String?, stateName: String?, state: Boolean) {
            subordinateProxies.forEachSafe { it.value.dispatchState(scene, stateName, state) }
        }

        fun dispatchKill(scene: String?, targetProcess: String?, targetPid: Int) {
            subordinateProxies.forEachSafe {
                it.value.dispatchKill(
                    scene,
                    targetProcess,
                    targetPid
                )
            }
        }

        fun dispatchDeath(
            scene: String?,
            targetProcess: String?,
            targetPid: Int,
            isLruKill: Boolean
        ) {
            subordinateProxies.forEachSafe {
                it.value.dispatchDeath(
                    scene,
                    targetProcess,
                    targetPid,
                    isLruKill
                )
            }
        }

        fun getPss(): Int {
            return 0
        }
    }

    private val dyingListeners = ArrayList<(recentScene: String?, processName: String?, pid: Int?) -> Boolean>()

    private fun ArrayList<(recentScene: String?, processName: String?, pid: Int?) -> Boolean>.invokeAll(
        recentScene: String?,
        processName: String?,
        pid: Int?
    ): Boolean {
        var rescue = false
        forEach {
            safeApply(ProcessSupervisor.tag) {
                val r = it.invoke(recentScene, processName, pid)
                if (r) {
                    MatrixLog.e(ProcessSupervisor.tag, "${it.javaClass} try to rescue process")
                }
                rescue = rescue || r
            }
        }
        return rescue
    }

    private val deathListeners =
        ArrayList<(recentScene: String?, processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit>()

    private fun ArrayList<(recentScene: String?, processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit>.invokeAll(
        recentScene: String?,
        processName: String?,
        pid: Int?,
        isLruKill: Boolean?
    ) = forEach {
        safeApply(ProcessSupervisor.tag) {
            it.invoke(recentScene, processName, pid, isLruKill)
        }
    }

    internal val processListener = object : IProcessListener {

        override fun addDyingListener(listener: (recentScene: String?, processName: String?, pid: Int?) -> Boolean) {
            dyingListeners.add(listener)
        }

        override fun removeDyingListener(listener: (recentScene: String?, processName: String?, pid: Int?) -> Boolean) {
            dyingListeners.remove(listener)
        }

        override fun addDeathListener(listener: (scene: String?, processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit) {
            deathListeners.add(listener)
        }

        override fun removeDeathListener(listener: (scene: String?, processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit) {
            deathListeners.remove(listener)
        }

        override fun getRecentScene() = SupervisorService.recentScene.let {
            if (!TextUtils.isEmpty(it)) {
                it
            } else {
                safeLet(ProcessSupervisor.tag, defVal = "") {
                    ProcessSupervisor.supervisorProxy?.recentScene ?: ""
                }
            }
        }
    }

    // ISubordinateProxy

    internal fun getSubordinate(app: Application): ISubordinateProxy.Stub = Subordinate(app)

    class Subordinate(val app: Application) : ISubordinateProxy.Stub() {

        private var rescued: Boolean = false

        override fun dispatchState(scene: String, stateName: String, state: Boolean) {
            safeApply(TAG) {
                if (state) {
                    DispatcherStateOwner.dispatchOn(stateName)
                } else {
                    DispatcherStateOwner.dispatchOff(stateName)
                }
            }
        }

        override fun dispatchKill(scene: String, targetProcess: String, targetPid: Int) {
            safeApply(TAG) {
                MatrixLog.d(ProcessSupervisor.tag, "receive kill target: $targetPid-$targetProcess")
                val toRescue = dyingListeners.invokeAll(scene, targetProcess, targetPid)
                if (targetProcess == MatrixUtil.getProcessName(app) && Process.myPid() == targetPid) {
                    val token = ProcessToken.current(app)
                    if (toRescue && rescued) {
                        rescued = true
                        ProcessSupervisor.supervisorProxy?.onProcessRescuedFromKill(token)
                        MatrixLog.e(ProcessSupervisor.tag, "rescued once !!!")
                        return
                    }

                    MatrixHandlerThread.getDefaultHandler().postDelayed({
                        if (!ProcessUILifecycleOwner.startedStateOwner.active()
                            && !ProcessUILifecycleOwner.hasForegroundService()
                            && !OverlayWindowLifecycleOwner.hasVisibleWindow()
                        ) {
                            ProcessSupervisor.supervisorProxy?.onProcessKilled(token)
                            MatrixLog.e(
                                ProcessSupervisor.tag,
                                "actual kill !!! supervisor = ${ProcessSupervisor.supervisorProxy}"
                            )
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                                ProcessUILifecycleOwner.getRunningAppTasksOf(
                                    MatrixUtil.getProcessName(app)
                                ).forEach {
                                    MatrixLog.e(
                                        ProcessSupervisor.tag,
                                        "removed task ${it.taskInfo.contentToString()}"
                                    )
                                    it.finishAndRemoveTask()
                                }
                            }
                            Process.killProcess(Process.myPid())
                        } else {
                            ProcessSupervisor.supervisorProxy?.onProcessKillCanceled(token)
                            MatrixLog.i(ProcessSupervisor.tag, "recheck: process is on foreground")
                        }
                    }, TimeUnit.SECONDS.toMillis(10))
                }
            }
        }

        override fun dispatchDeath(
            scene: String,
            targetProcess: String,
            targetPid: Int,
            isLruKill: Boolean
        ) {
            safeApply(TAG) {
                deathListeners.invokeAll(scene, targetProcess, targetPid, isLruKill)
            }
        }

        override fun getPss(): Int {
            TODO("Not yet implemented")
        }

    }

}