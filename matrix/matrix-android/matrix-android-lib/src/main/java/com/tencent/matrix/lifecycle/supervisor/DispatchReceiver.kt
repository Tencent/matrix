package com.tencent.matrix.lifecycle.supervisor

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Process
import com.tencent.matrix.lifecycle.owners.MatrixProcessLifecycleOwner
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import com.tencent.matrix.util.contentToString
import java.util.concurrent.TimeUnit

/**
 * [DispatchReceiver] should be installed in all processes
 * by calling [ProcessSupervisor.initSupervisor]
 *
 * Created by Yves on 2021/10/12
 */
internal object DispatchReceiver : BroadcastReceiver() {

    private const val KEY_PROCESS_NAME = "KEY_PROCESS_NAME"
    private const val KEY_PROCESS_PID = "KEY_PROCESS_PID"
    private const val KEY_STATEFUL_NAME = "KEY_STATEFUL_NAME"

    private var packageName: String? = null
    private val permission by lazy { "${packageName!!}.matrix.permission.PROCESS_SUPERVISOR" }

    private var rescued: Boolean = false
    private val killedListeners = ArrayList<(isCurrent: Boolean) -> Boolean>()

    private fun ArrayList<(isCurrent: Boolean) -> Boolean>.invokeAll(isCurrent: Boolean): Boolean {
        var rescue = false
        forEach {
            val r = it.invoke(isCurrent)
            if (r) {
                MatrixLog.e(ProcessSupervisor.tag, "${it.javaClass} try to rescue process")
            }
            rescue = rescue || r
        }
        return rescue
    }

    private enum class SupervisorEvent {
        SUPERVISOR_DISPATCH_APP_STATE_TURN_ON,
        SUPERVISOR_DISPATCH_APP_STATE_TURN_OFF,
        SUPERVISOR_DISPATCH_KILL;
    }

    internal fun install(context: Context?) {
        packageName = MatrixUtil.getPackageName(context)
        if (ProcessSupervisor.isSupervisor) {
            return
        }
        val filter = IntentFilter()
        SupervisorEvent.values().forEach {
            filter.addAction(it.name)
        }
        context?.registerReceiver(
            this, filter,
            permission, null
        )
        MatrixLog.i(ProcessSupervisor.tag, "DispatchReceiver installed")
    }

    fun addKilledListener(listener: (isCurrent: Boolean) -> Boolean) {
        killedListeners.add(listener)
    }

    fun removeKilledListener(listener: (isCurrent: Boolean) -> Boolean) {
        killedListeners.remove(listener)
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

    // call from supervisor process
    private fun dispatch(
        context: Context?,
        event: SupervisorEvent,
        vararg params: Pair<String, String>
    ) {
//        MatrixLog.d(SupervisorLifecycle.tag, "dispatch [${event.name}]")
        val intent = Intent(event.name)
        params.forEach {
            intent.putExtra(it.first, it.second)
        }
        context?.sendBroadcast(intent, permission)
    }

    override fun onReceive(context: Context, intent: Intent?) {
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
                if (targetProcessName == MatrixUtil.getProcessName(context) && Process.myPid()
                        .toString() == targetPid
                ) {
                    val token = ProcessToken.current(context)
                    if (killedListeners.invokeAll(true) && !rescued) {
                        rescued = true
                        ProcessSupervisor.supervisorProxy?.onProcessRescuedFromKill(token)
                        MatrixLog.e(ProcessSupervisor.tag, "rescued once !!!")
                        return
                    }

                    MatrixHandlerThread.getDefaultHandler().postDelayed({
                        if (!MatrixProcessLifecycleOwner.startedStateOwner.active()
                            && !MatrixProcessLifecycleOwner.hasForegroundService()
                            && !MatrixProcessLifecycleOwner.hasVisibleView()
                        ) {
                            ProcessSupervisor.supervisorProxy?.onProcessKilled(token)
                            MatrixLog.e(ProcessSupervisor.tag, "actual kill !!! supervisor = ${ProcessSupervisor.supervisorProxy}")
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                                MatrixProcessLifecycleOwner.getRunningAppTasksOf(MatrixUtil.getProcessName(context)).forEach {
                                    it.finishAndRemoveTask()
                                    MatrixLog.e(ProcessSupervisor.tag, "removed task ${it.taskInfo.contentToString()}")
                                }
                            }
                            Process.killProcess(Process.myPid())
                        } else {
                            ProcessSupervisor.supervisorProxy?.onProcessKillCanceled(token)
                            MatrixLog.i(ProcessSupervisor.tag, "recheck: process is on foreground")
                        }
                    }, TimeUnit.SECONDS.toMillis(10))
                } else {
                    killedListeners.invokeAll(false)
                }
            }
        }
    }
}