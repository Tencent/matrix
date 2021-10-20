package com.tencent.matrix.memory.canary.lifecycle.supervisor

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Process
import com.tencent.matrix.memory.canary.lifecycle.owners.CombinedProcessForegroundStatefulOwner
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
    internal var killedListener: (() -> Boolean)? = null

    private enum class SupervisorEvent {
        SUPERVISOR_INSTALLED,
        SUPERVISOR_DISPATCH_APP_FOREGROUND,
        SUPERVISOR_DISPATCH_APP_BACKGROUND,
        SUPERVISOR_DISPATCH_KILL;
    }

    internal fun install(context: Context?) {
        if (SupervisorReceiver.isSupervisor) {
            return
        }
        val filter = IntentFilter()
        SupervisorEvent.values().forEach {
            filter.addAction(it.name)
        }
        context?.registerReceiver(
            this, filter,
            SUPERVISOR_PERMISSION, null
        )
//        MatrixLog.i(SupervisorLifecycleOwner.tag, "DispatchReceiver installed")
    }

    internal fun dispatchSuperVisorInstalled(context: Context?) {
        dispatch(context, SupervisorEvent.SUPERVISOR_INSTALLED)
    }

    internal fun dispatchAppForeground(context: Context?) {
        dispatch(context, SupervisorEvent.SUPERVISOR_DISPATCH_APP_FOREGROUND)
    }

    internal fun dispatchAppBackground(context: Context?) {
        dispatch(context, SupervisorEvent.SUPERVISOR_DISPATCH_APP_BACKGROUND)
    }

    internal fun dispatchKill(context: Context?, targetProcessName: String) {
        dispatch(
            context,
            SupervisorEvent.SUPERVISOR_DISPATCH_KILL,
            Pair(KEY_PROCESS_NAME, targetProcessName)
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
        context?.sendBroadcast(intent, SUPERVISOR_PERMISSION)
    }

    override fun onReceive(context: Context?, intent: Intent?) {
        when (intent?.action) {
            SupervisorEvent.SUPERVISOR_INSTALLED.name -> {
                SupervisorReceiver.flushPending(context)
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_FOREGROUND.name -> {
                ProcessSupervisor.syncAppForeground()
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_APP_BACKGROUND.name -> {
                ProcessSupervisor.syncAppBackground()
            }
            SupervisorEvent.SUPERVISOR_DISPATCH_KILL.name -> {
                val target = intent.getStringExtra(KEY_PROCESS_NAME)
                MatrixLog.d(ProcessSupervisor.tag, "receive kill target: $target")
                if (target == MatrixUtil.getProcessName(context)) {

                    if (killedListener?.invoke() == true && !rescued) {
                        rescued = true
                        SupervisorReceiver.sendOnProcessKillCanceled(context)
                        MatrixLog.e(ProcessSupervisor.tag, "rescued once !!!")
                        return
                    }

                    MatrixHandlerThread.getDefaultHandler().postDelayed({
                        if (!CombinedProcessForegroundStatefulOwner.active()) {
                            SupervisorReceiver.sendOnProcessKilled(context)
                            MatrixLog.e(ProcessSupervisor.tag, "actual kill !!!")
                            Process.killProcess(Process.myPid())
                        } else {
                            SupervisorReceiver.sendOnProcessKillCanceled(context)
                            MatrixLog.i(ProcessSupervisor.tag, "recheck: process is on foreground")
                        }
                    }, TimeUnit.SECONDS.toMillis(10))
                }
            }
        }
    }
}