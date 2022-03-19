package com.tencent.matrix.lifecycle.supervisor

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Process
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner
import com.tencent.matrix.util.*

/**
 * Created by Yves on 2021/10/12
 */
internal object SupervisorPacemaker : BroadcastReceiver() {

    private const val KEY_PROCESS_NAME = "KEY_PROCESS_NAME"
    private const val KEY_PROCESS_PID = "KEY_PROCESS_PID"

    private const val TELL_SUPERVISOR_FOREGROUND = "TELL_SUPERVISOR_FOREGROUND"

    private var packageName: String? = null
    private val permission by lazy { "${packageName!!}.matrix.permission.PROCESS_SUPERVISOR" }

    @Volatile
    private var pacemaker: IStateObserver? = null

    private fun installPacemaker(context: Context?) {
        if (pacemaker == null) {
            pacemaker = object : IStateObserver {
                override fun on() {
                    MatrixLog.i(ProcessSupervisor.tag, "pacemaker: call supervisor")
                    if (ProcessSupervisor.config!!.autoCreate) {
                        SupervisorService.start(context!!)
                    } else {
                        tellSupervisorForeground(context)
                    }
                }

                override fun off() {
                }
            }
            ProcessUILifecycleOwner.startedStateOwner.observeForever(pacemaker!!)
            MatrixLog.i(ProcessSupervisor.tag, "pacemaker: install pacemaker")
        }
    }

    internal fun uninstall() {
        if (pacemaker != null) {
            ProcessUILifecycleOwner.startedStateOwner.removeObserver(pacemaker!!)
            pacemaker = null
            MatrixLog.i(ProcessSupervisor.tag, "pacemaker: uninstall pacemaker")
        }
    }

    internal fun install(context: Context?) {
        packageName = MatrixUtil.getPackageName(context)
        val filter = IntentFilter()
        if (ProcessSupervisor.isSupervisor) {
            filter.addAction(TELL_SUPERVISOR_FOREGROUND)
            context?.registerReceiver(this, filter, permission, null)
            MatrixLog.i(ProcessSupervisor.tag, "pacemaker: receiver installed")
        } else {
            installPacemaker(context)
        }
    }

    private fun tellSupervisorForeground(context: Context?) {
        Intent(TELL_SUPERVISOR_FOREGROUND).apply {
            putExtra(KEY_PROCESS_NAME, MatrixUtil.getProcessName(context))
            putExtra(KEY_PROCESS_PID, Process.myPid())
            context?.sendBroadcast(this, permission)
        }
    }

    override fun onReceive(context: Context, intent: Intent?) {
        when (intent?.action) {
            TELL_SUPERVISOR_FOREGROUND -> {
                if (false == ProcessSupervisor.config?.enable) {
                    MatrixLog.e(ProcessSupervisor.tag, "supervisor was disabled")
                    return
                }
                if (!ProcessSupervisor.isSupervisor) {
                    MatrixLog.e(ProcessSupervisor.tag, "ERROR: this is NOT supervisor process")
                    return
                }
                val processName = intent.getStringExtra(KEY_PROCESS_NAME)
                val pid = intent.getIntExtra(KEY_PROCESS_PID, -1)
                MatrixLog.i(
                    ProcessSupervisor.tag,
                    "pacemaker: receive TELL_SUPERVISOR_FOREGROUND from $pid-$processName"
                )
                SupervisorService.start(context.applicationContext)
            }
        }
    }
}