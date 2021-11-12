package com.tencent.matrix.lifecycle.supervisor

import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.MatrixHandlerThread
import com.tencent.matrix.util.MatrixLog
import junit.framework.Assert
import java.util.*
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ConcurrentLinkedQueue

/**
 * Created by Yves on 2021/11/11
 */
class SupervisorService : Service() {

    companion object {
        private const val TAG = "Matrix.ProcessSupervisor.SupervisorService"
        var isSupervisor = false
            private set
    }

    private class TokenRecord {
        private val pidToToken: ConcurrentHashMap<Int, ProcessToken>
                by lazy(LazyThreadSafetyMode.SYNCHRONIZED) { ConcurrentHashMap() }
        private val nameToToken: ConcurrentHashMap<String, ProcessToken>
                by lazy(LazyThreadSafetyMode.SYNCHRONIZED) { ConcurrentHashMap() }

        fun addToken(token: ProcessToken) {
            pidToToken[token.pid] = token
            nameToToken[token.name] = token
        }

        fun getToken(pid: Int) = pidToToken[pid]
        fun getToken(name: String) = nameToToken[name]

        fun removeToken(pid: Int): ProcessToken {
            val rm = pidToToken.remove(pid)
            rm?.let {
                nameToToken.remove(rm.name)
                return rm
            } ?: throw IllegalStateException("token with pid=$pid not found")
        }

        fun removeToken(name: String): ProcessToken {
            val rm = nameToToken.remove(name)
            rm?.let {
                pidToToken.remove(rm.pid)
                return rm
            } ?: throw IllegalStateException("token with name=$name not found")
        }
    }

    private val tokenRecord: TokenRecord = TokenRecord()

    private val backgroundProcessLru: ConcurrentLinkedQueue<ProcessToken>
            by lazy(LazyThreadSafetyMode.SYNCHRONIZED) { ConcurrentLinkedQueue() }

    private fun ConcurrentLinkedQueue<ProcessToken>.contentToString(): String {
        val it: Iterator<ProcessToken> = iterator()
        if (!it.hasNext()) return "[]"

        val sb = StringBuilder()
        sb.append('[')
        while (true) {
            val e: ProcessToken = it.next()
            sb.append("${e.pid}-${e.name}")
            if (!it.hasNext()) return sb.append(']').toString()
            sb.append(',').append(' ')
        }
    }

    private fun ConcurrentLinkedQueue<ProcessToken>.moveOrAddFirst(token: ProcessToken) {
        remove(token)
        add(token)
    }

    override fun onCreate() {
        super.onCreate()

        MatrixLog.d(TAG, "onCreate")
        isSupervisor = true
        ProcessSupervisor.observeForever(object : IStateObserver {
            override fun on() {
                DispatchReceiver.dispatchAppForeground(this@SupervisorService)
            }

            override fun off() {
                DispatchReceiver.dispatchAppBackground(this@SupervisorService)
            }
        })
    }

    override fun onBind(intent: Intent?): IBinder {
        MatrixLog.d(TAG, "onBind")
        return binder
    }

    private fun asyncLog(format: String?, vararg obj: Any?) {
        MatrixHandlerThread.getDefaultHandler().post {
            MatrixLog.i(TAG, format, *obj)
        }
    }

    val binder = object : ISupervisorProxy.Stub() {
        override fun onProcessCreate(token: ProcessToken) {
            val pid = Binder.getCallingPid()
            Assert.assertEquals(pid, token.pid)
            token.linkToDeath {
                val dead = tokenRecord.removeToken(pid)
                backgroundProcessLru.remove(dead)
                RemoteProcessLifecycleProxy.removeProxy(dead)
                MatrixLog.i(TAG, "$pid-$dead was dead")
            }
            tokenRecord.addToken(token)
            RemoteProcessLifecycleProxy.getProxy(token)
            backgroundProcessLru.moveOrAddFirst(token)
            asyncLog(
                "CREATED: [$pid-${token.name}] -> [${backgroundProcessLru.size}]${backgroundProcessLru.contentToString()}"
            )
        }

        override fun onProcessForeground(token: ProcessToken) {
            val pid = Binder.getCallingPid()
            Assert.assertEquals(pid, token.pid)

            backgroundProcessLru.remove(token)
            RemoteProcessLifecycleProxy.getProxy(token).onRemoteProcessForeground()

            asyncLog(
                "FOREGROUND: [$pid-${token.name}] <- [${backgroundProcessLru.size}]${backgroundProcessLru.contentToString()}"
            )
        }

        override fun onProcessBackground(token: ProcessToken) {
            val pid = Binder.getCallingPid()
            Assert.assertEquals(pid, token.pid)

            backgroundProcessLru.add(token)
            RemoteProcessLifecycleProxy.getProxy(token).onRemoteProcessBackground()

            asyncLog(
                "BACKGROUND: [$pid-${token.name}] -> [${backgroundProcessLru.size}]${backgroundProcessLru.contentToString()}"
            )
        }

        override fun onProcessKilled(token: ProcessToken) {
            val pid = Binder.getCallingPid()
            Assert.assertEquals(pid, token.pid)
            TODO("Not yet implemented")
        }

        override fun onProcessRescuedFromKill(token: ProcessToken) {
            val pid = Binder.getCallingPid()
            Assert.assertEquals(pid, token.pid)
            TODO("Not yet implemented")
        }

        override fun onProcessKillCanceled(token: ProcessToken) {
            val pid = Binder.getCallingPid()
            Assert.assertEquals(pid, token.pid)
            TODO("Not yet implemented")
        }
    }

    private class RemoteProcessLifecycleProxy(val token: ProcessToken) :
        StatefulOwner() {

        init {
            ProcessSupervisor.addSourceOwner(this)
        }

        companion object {
            private val processFgObservers by lazy { ConcurrentHashMap<ProcessToken, RemoteProcessLifecycleProxy>() }

            fun getProxy(token: ProcessToken) =
                processFgObservers.getOrPut(token, { RemoteProcessLifecycleProxy(token) })!!


            fun removeProxy(token: ProcessToken) {
                processFgObservers.remove(token)?.let { statefulOwner ->
                    ProcessSupervisor.removeSourceOwner(statefulOwner)
                }
            }
        }

        fun onRemoteProcessForeground() = turnOn()
        fun onRemoteProcessBackground() = turnOff()
        override fun toString(): String {
            return "OwnerProxy_${token.name}_${token.pid}"
        }
    }
}