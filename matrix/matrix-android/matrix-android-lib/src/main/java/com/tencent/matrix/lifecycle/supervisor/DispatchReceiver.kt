package com.tencent.matrix.lifecycle.supervisor

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Process
import com.tencent.matrix.lifecycle.owners.CombinedProcessForegroundOwner
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import com.tencent.matrix.util.MatrixUtil
import java.util.concurrent.TimeUnit

/**
 * [DispatchReceiver] should be installed in all processes
 * by calling [ProcessSupervisor.initSupervisor]
 *
 * Created by Yves on 2021/10/12
 */
internal object DispatchReceiver : BroadcastReceiver() {

    private var rescued: Boolean = false
    private val killedListeners = ArrayList<() -> Boolean>()

    private fun ArrayList<() -> Boolean>.invokeAll(): Boolean {
        var rescue = false
        forEach {
            val r = it.invoke()
            if (r) {
                MatrixLog.e(ProcessSupervisor.tag, "${it.javaClass} try to rescue process")
            }
            rescue = rescue || r
        }
        return rescue
    }

    private enum class SupervisorEvent {
//        SUPERVISOR_INSTALLED,
        SUPERVISOR_DISPATCH_APP_FOREGROUND,
        SUPERVISOR_DISPATCH_APP_BACKGROUND,
        SUPERVISOR_DISPATCH_KILL;
    }

    internal fun install(context: Context?) {
        if (ProcessSupervisor.isSupervisor) {
            return
        }
        val filter = IntentFilter()
        SupervisorEvent.values().forEach {
            filter.addAction(it.name)
        }
        context?.registerReceiver(
            this, filter,
            ProcessSupervisor.permission, null
        )
//        MatrixLog.i(SupervisorLifecycleOwner.tag, "DispatchReceiver installed")
    }

    fun addKilledListener(listener: () -> Boolean) {
        killedListeners.add(listener)
    }

    fun removeKilledListener(listener: () -> Boolean) {
        killedListeners.remove(listener)
    }

    internal fun dispatchAppForeground(context: Context?) {
        dispatch(context, SupervisorEvent.SUPERVISOR_DISPATCH_APP_FOREGROUND)
    }

    internal fun dispatchAppBackground(context: Context?) {
        dispatch(context, SupervisorEvent.SUPERVISOR_DISPATCH_APP_BACKGROUND)
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
        context?.sendBroadcast(intent, ProcessSupervisor.permission)
    }

    override fun onReceive(context: Context, intent: Intent?) {
        when (intent?.action) {
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_FOREGROUND.name -> {
                ProcessSupervisor.syncAppForeground()
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_BACKGROUND.name -> {
                ProcessSupervisor.syncAppBackground()
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
                    if (killedListeners.invokeAll() && !rescued) {
                        rescued = true
                        ProcessSupervisor.supervisorProxy?.onProcessRescuedFromKill(token)
                        MatrixLog.e(ProcessSupervisor.tag, "rescued once !!!")
                        return
                    }

                    MatrixHandlerThread.getDefaultHandler().postDelayed({
                        if (!CombinedProcessForegroundOwner.active()) {
                            ProcessSupervisor.supervisorProxy?.onProcessKilled(token)
                            MatrixLog.e(ProcessSupervisor.tag, "actual kill !!!")
                            Process.killProcess(Process.myPid())
                        } else {
                            ProcessSupervisor.supervisorProxy?.onProcessKillCanceled(token)
                            MatrixLog.i(ProcessSupervisor.tag, "recheck: process is on foreground")
                        }
                    }, TimeUnit.SECONDS.toMillis(10))
                }
            }
        }
    }
}