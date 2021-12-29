package com.tencent.matrix.lifecycle.supervisor

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Process
import android.text.TextUtils
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.MatrixProcessLifecycleOwner
import com.tencent.matrix.util.*
import java.util.concurrent.TimeUnit

internal interface IProcessListener {
    fun addDyingListener(listener: (processName: String?, pid: Int?) -> Boolean)
    fun removeDyingListener(listener: (processName: String?, pid: Int?) -> Boolean)
    fun addDeathListener(listener: (processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit)
    fun removeDeathListener(listener: (processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit)
    fun recentScene(): String
}

/**
 * [DispatchReceiver] should be installed in all processes
 * by calling [ProcessSupervisor.initSupervisor]
 *
 * todo: refactor - impl by aidl ipc instead
 *
 * Created by Yves on 2021/10/12
 */
internal object DispatchReceiver : BroadcastReceiver(), IProcessListener {

    private const val KEY_PROCESS_NAME = "KEY_PROCESS_NAME"
    private const val KEY_PROCESS_PID = "KEY_PROCESS_PID"
    private const val KEY_STATEFUL_NAME = "KEY_STATEFUL_NAME"
    private const val KEY_DEAD_FROM_LRU_KILL = "KEY_DEAD_FROM_LRU_KILL"
    private const val KEY_GLOBAL_SCENE = "KEY_GLOBAL_SCENE"

    private var packageName: String? = null
    private val permission by lazy { "${packageName!!}.matrix.permission.PROCESS_SUPERVISOR" }

    private var rescued: Boolean = false
    private val dyingListeners = ArrayList<(processName: String?, pid: Int?) -> Boolean>()
    private val deathListeners =
        ArrayList<(processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit>()

    @Volatile
    private var supervisorInstalled = false

    @Volatile
    private var pacemaker: IStateObserver? = null

    @Volatile
    private var recentScene = ""

    private fun ArrayList<(processName: String?, pid: Int?) -> Boolean>.invokeAll(
        processName: String?,
        pid: Int?
    ): Boolean {
        var rescue = false
        forEach {
            safeApply(ProcessSupervisor.tag) {
                val r = it.invoke(processName, pid)
                if (r) {
                    MatrixLog.e(ProcessSupervisor.tag, "${it.javaClass} try to rescue process")
                }
                rescue = rescue || r
            }
        }
        return rescue
    }

    private fun ArrayList<(processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit>.invokeAll(
        processName: String?,
        pid: Int?,
        isLruKill: Boolean?
    ) =
        forEach {
            safeApply(ProcessSupervisor.tag) {
                it.invoke(processName, pid, isLruKill)
            }
        }


    private enum class SupervisorEvent {
        SUPERVISOR_DISPATCH_APP_STATE_TURN_ON,
        SUPERVISOR_DISPATCH_APP_STATE_TURN_OFF,
        SUPERVISOR_DISPATCH_KILL,
        SUPERVISOR_DISPATCH_DEATH,
        TELL_SUPERVISOR_FOREGROUND;
    }

    private fun installPacemaker(context: Context?) {
        if (pacemaker == null && !supervisorInstalled) {
            pacemaker = object : IStateObserver {
                override fun on() {
                    if (!supervisorInstalled) {
                        MatrixLog.i(ProcessSupervisor.tag, "pacemaker: call supervisor")
                        if (ProcessSupervisor.config!!.autoCreate) {
                            SupervisorService.start(context!!)
                        } else {
                            tellSupervisorForeground(context)
                        }
                    }
                }

                override fun off() {
                }
            }
            MatrixProcessLifecycleOwner.startedStateOwner.observeForever(pacemaker!!)
            MatrixLog.i(ProcessSupervisor.tag, "pacemaker: install pacemaker")
        }
    }

    internal fun uninstallPacemaker() {
        supervisorInstalled = true
        if (pacemaker != null) {
            MatrixProcessLifecycleOwner.startedStateOwner.removeObserver(pacemaker!!)
            pacemaker = null
            MatrixLog.i(ProcessSupervisor.tag, "pacemaker: uninstall pacemaker")
        }
    }

    internal fun install(context: Context?) {
        packageName = MatrixUtil.getPackageName(context)
        val filter = IntentFilter()
        if (ProcessSupervisor.isSupervisor) {
            filter.addAction(SupervisorEvent.TELL_SUPERVISOR_FOREGROUND.name)
        } else {
            SupervisorEvent.values()
                .filter { it != SupervisorEvent.TELL_SUPERVISOR_FOREGROUND }
                .forEach { filter.addAction(it.name) }
            installPacemaker(context)
        }
        context?.registerReceiver(
            this, filter,
            permission, null
        )
        MatrixLog.i(ProcessSupervisor.tag, "DispatchReceiver installed")
    }

    override fun addDyingListener(listener: (processName: String?, pid: Int?) -> Boolean) {
        dyingListeners.add(listener)
    }

    override fun removeDyingListener(listener: (processName: String?, pid: Int?) -> Boolean) {
        dyingListeners.remove(listener)
    }

    override fun addDeathListener(listener: (processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit) {
        deathListeners.add(listener)
    }

    override fun removeDeathListener(listener: (processName: String?, pid: Int?, isLruKill: Boolean?) -> Unit) {
        deathListeners.remove(listener)
    }

    override fun recentScene() = SupervisorService.recentScene.let {
        if (!TextUtils.isEmpty(it)) {
            it
        } else {
            recentScene
        }
    }

    internal fun dispatchAppStateOn(context: Context?, statefulName: String) {
        dispatch(
            context,
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_STATE_TURN_ON,
            KEY_STATEFUL_NAME to statefulName
        )
    }

    internal fun dispatchAppStateOff(context: Context?, statefulName: String) {
        dispatch(
            context,
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_STATE_TURN_OFF,
            KEY_STATEFUL_NAME to statefulName
        )
    }

    internal fun dispatchKill(context: Context?, targetProcessName: String, targetPid: Int) {
        dispatch(
            context,
            SupervisorEvent.SUPERVISOR_DISPATCH_KILL,
            KEY_PROCESS_NAME to targetProcessName,
            KEY_PROCESS_PID to targetPid.toString()
        )
    }

    internal fun dispatchDeath(
        context: Context?,
        processName: String,
        pid: Int,
        deadFromLruKill: Boolean
    ) {
        dispatch(
            context,
            SupervisorEvent.SUPERVISOR_DISPATCH_DEATH,
            KEY_PROCESS_NAME to processName,
            KEY_PROCESS_PID to pid.toString(),
            KEY_DEAD_FROM_LRU_KILL to deadFromLruKill.toString()
        )
    }

    private fun tellSupervisorForeground(context: Context?) {
        dispatch(
            context, SupervisorEvent.TELL_SUPERVISOR_FOREGROUND,
            KEY_PROCESS_NAME to MatrixUtil.getProcessName(context),
            KEY_PROCESS_PID to Process.myPid().toString()
        )
    }

    // call from supervisor process
    private fun dispatch(
        context: Context?,
        event: SupervisorEvent,
        vararg params: Pair<String, String>
    ) {
        val intent = Intent(event.name)
        params.forEach {
            intent.putExtra(it.first, it.second)
        }
        intent.putExtra(KEY_GLOBAL_SCENE, SupervisorService.recentScene) // spread the recent scene
        context?.sendBroadcast(intent, permission)
    }

    override fun onReceive(context: Context, intent: Intent?) {
        recentScene = intent?.getStringExtra(KEY_GLOBAL_SCENE) ?: recentScene

        when (intent?.action) {
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_STATE_TURN_ON.name -> {
                val statefulName = intent.getStringExtra(KEY_STATEFUL_NAME)
                statefulName?.let {
                    DispatcherStateOwner.dispatchOn(it)
                }
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_STATE_TURN_OFF.name -> {
                val statefulName = intent.getStringExtra(KEY_STATEFUL_NAME)
                statefulName?.let {
                    DispatcherStateOwner.dispatchOff(it)
                }
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_KILL.name -> {
                val targetProcessName = intent.getStringExtra(KEY_PROCESS_NAME)
                val targetPid = intent.getStringExtra(KEY_PROCESS_PID)
                MatrixLog.d(
                    ProcessSupervisor.tag,
                    "receive kill target: $targetPid-$targetProcessName"
                )

                val toRescue = dyingListeners.invokeAll(targetProcessName, targetPid?.toInt())

                if (targetProcessName == MatrixUtil.getProcessName(context)
                    && Process.myPid().toString() == targetPid
                ) {
                    val token = ProcessToken.current(context)
                    if (toRescue && !rescued) {
                        rescued = true
                        ProcessSupervisor.supervisorProxy?.onProcessRescuedFromKill(token)
                        MatrixLog.e(ProcessSupervisor.tag, "rescued once !!!")
                        return
                    }

                    MatrixHandlerThread.getDefaultHandler().postDelayed({
                        if (!MatrixProcessLifecycleOwner.startedStateOwner.active()
                            && !MatrixProcessLifecycleOwner.hasForegroundService()
                            && !MatrixProcessLifecycleOwner.hasVisibleWindow()
                        ) {
                            ProcessSupervisor.supervisorProxy?.onProcessKilled(token)
                            MatrixLog.e(
                                ProcessSupervisor.tag,
                                "actual kill !!! supervisor = ${ProcessSupervisor.supervisorProxy}"
                            )
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                                MatrixProcessLifecycleOwner.getRunningAppTasksOf(
                                    MatrixUtil.getProcessName(
                                        context
                                    )
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
            SupervisorEvent.SUPERVISOR_DISPATCH_DEATH.name -> {
                val processName = intent.getStringExtra(KEY_PROCESS_NAME)
                val pid = intent.getStringExtra(KEY_PROCESS_PID)?.toInt()
                val deadFromLruKill = intent.getStringExtra(KEY_DEAD_FROM_LRU_KILL).toBoolean()
                deathListeners.invokeAll(processName, pid, deadFromLruKill)
            }
            SupervisorEvent.TELL_SUPERVISOR_FOREGROUND.name -> {
                if (!ProcessSupervisor.isSupervisor) {
                    MatrixLog.e(ProcessSupervisor.tag, "ERROR: this is NOT supervisor process")
                    return
                }
                val processName = intent.getStringExtra(KEY_PROCESS_NAME)
                val pid = intent.getStringExtra(KEY_PROCESS_PID)?.toInt()
                MatrixLog.i(
                    ProcessSupervisor.tag,
                    "pacemaker: receive TELL_SUPERVISOR_FOREGROUND from $pid-$processName"
                )
                SupervisorService.start(context.applicationContext)
            }
        }
    }
}