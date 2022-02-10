package com.tencent.matrix.lifecycle.supervisor

import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.*
import com.tencent.matrix.lifecycle.MatrixLifecycleThread
import com.tencent.matrix.lifecycle.StatefulOwner
import com.tencent.matrix.util.*
import junit.framework.Assert
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ConcurrentLinkedQueue

/**
 * Created by Yves on 2021/11/11
 */
class SupervisorService : Service() {

    companion object {
        private const val TAG = "Matrix.ProcessSupervisor.Service"

        @Volatile
        internal var isSupervisor = false
            private set

        @Volatile
        internal var recentScene: String = ""

        internal fun start(context: Context) = safeApply(TAG) {
            if (instance != null) {
                MatrixLog.e(TAG, "duplicated start")
                return@safeApply
            }
            val intent = Intent(context, SupervisorService::class.java)
            context.startService(intent)
        }

        @Volatile
        internal var instance: SupervisorService? = null
            private set
    }

    private val runningHandler = MatrixLifecycleThread.handler

    private val tokenRecord: TokenRecord = TokenRecord()

    private val backgroundProcessLru by lazy(LazyThreadSafetyMode.SYNCHRONIZED) { ConcurrentLinkedQueue<ProcessToken>() }

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

    private var targetKilledCallback: ((result: Int, target: String?, pid: Int) -> Unit)? = null

    private val binder = object : ISupervisorProxy.Stub() {
        override fun registerSubordinate(
            tokens: Array<ProcessToken>,
            subordinateProxy: ISubordinateProxy
        ) {
            val pid = Binder.getCallingPid()

            runningHandler.post {
                MatrixLog.d(
                    TAG,
                    "supervisor called register, tokens(${tokens.size}): ${tokens.contentToString()}"
                )

                tokens.first().apply {
                    tokenRecord.addToken(this)
                    ProcessSubordinate.manager.addProxy(this, subordinateProxy)
                    backgroundProcessLru.moveOrAddFirst(this)
                    MatrixLog.i(
                        TAG,
                        "CREATED: [${this.pid}-${name}] -> [${backgroundProcessLru.size}]${backgroundProcessLru.contentToString()}"
                    )

                    safeApply(TAG) {
                        linkToDeath {
                            safeApply(TAG) {
                                val dead = tokenRecord.removeToken(pid)
                                val lruRemoveSuccess = backgroundProcessLru.remove(dead)
                                ProcessSubordinate.manager.removeProxy(dead)
                                val proxyRemoveSuccess =
                                    RemoteProcessLifecycleProxy.removeProxy(dead)
                                ProcessSubordinate.manager.dispatchDeath(
                                    recentScene,
                                    dead.name,
                                    dead.pid,
                                    !lruRemoveSuccess && !proxyRemoveSuccess
                                )
                                MatrixLog.i(
                                    TAG,
                                    "$pid-$dead was dead. is LRU kill? ${!lruRemoveSuccess && !proxyRemoveSuccess}"
                                )
                            }
                        }
                    }
                }

                tokens.forEach {
                    MatrixLog.d(TAG, "register: ${it.name}, ${it.statefulName}, ${it.state}")
                    RemoteProcessLifecycleProxy.getProxy(it).onStateChanged(it.state)
                }

                if (tokenRecord.isEmpty()) {
                    MatrixLog.i(
                        TAG,
                        "stateRegister: no other process registered, ignore state changes"
                    )
                    return@post
                }
                DispatcherStateOwner.syncStates(
                    ProcessToken.current(applicationContext),
                    recentScene
                ) // sync state for new process
            }
        }

        override fun onStateChanged(token: ProcessToken) {
            runningHandler.post {
                MatrixLog.i(
                    TAG,
                    "onStateChanged: ${token.statefulName} ${token.state} ${token.name}"
                )
                RemoteProcessLifecycleProxy.getProxy(token).onStateChanged(token.state)
                onProcessStateChanged(token)
            }
        }

        private fun onProcessStateChanged(token: ProcessToken) {
            if (ProcessSupervisor.EXPLICIT_BACKGROUND_OWNER == token.statefulName) {
                if (token.state) {
                    backgroundProcessLru.moveOrAddFirst(token)
                    MatrixLog.i(
                        TAG,
                        "BACKGROUND: [${token.pid}-${token.name}] -> [${backgroundProcessLru.size}]${backgroundProcessLru.contentToString()}"
                    )
                } else {
                    backgroundProcessLru.remove(token)
                    MatrixLog.i(
                        TAG,
                        "FOREGROUND: [${token.pid}-${token.name}] <- [${backgroundProcessLru.size}]${backgroundProcessLru.contentToString()}"
                    )
                }
            }
        }

        override fun onSceneChanged(scene: String) {
            SupervisorService.recentScene = scene
        }

        override fun onProcessKilled(token: ProcessToken) {
            runningHandler.post {
                safeApply(TAG) {
                    targetKilledCallback?.invoke(LRU_KILL_SUCCESS, token.name, token.pid)
                }
                backgroundProcessLru.remove(token)
                RemoteProcessLifecycleProxy.removeProxy(token)
                MatrixLog.i(
                    TAG,
                    "KILL: [${token.pid}-${token.name}] X [${backgroundProcessLru.size}]${backgroundProcessLru.contentToString()}"
                )
            }
        }

        override fun onProcessRescuedFromKill(token: ProcessToken) {
            runningHandler.post {
                safeApply(TAG) {
                    targetKilledCallback?.invoke(LRU_KILL_RESCUED, token.name, token.pid)
                }
            }
        }

        override fun onProcessKillCanceled(token: ProcessToken) {
            runningHandler.post {
                safeApply(TAG) {
                    targetKilledCallback?.invoke(LRU_KILL_CANCELED, token.name, token.pid)
                }
            }
        }

        override fun getRecentScene() = SupervisorService.recentScene
    }

    override fun onCreate() {
        super.onCreate()

        MatrixLog.d(TAG, "onCreate")
        isSupervisor = true
        instance = this


        DispatcherStateOwner.observe { stateName, state ->
            if (tokenRecord.isEmpty()) {
                MatrixLog.i(TAG, "observe: no other process registered, ignore state changes")
                return@observe
            }
            MatrixLog.d(TAG, "supervisor dispatch $stateName $state")
            ProcessSubordinate.manager.dispatchState(
                ProcessToken.current(applicationContext),
                recentScene,
                stateName,
                state
            )
        }

        SubordinatePacemaker.notifySupervisorInstalled(applicationContext)
    }

    override fun onBind(intent: Intent?): IBinder {
        MatrixLog.d(TAG, "onBind")
        return binder
    }

    internal fun backgroundLruKill(
        killedCallback: (result: Int, process: String?, pid: Int) -> Unit
    ) {
        if (!isSupervisor) {
            throw IllegalStateException("backgroundLruKill should only be called in supervisor")
        }

        if (instance == null) {
            throw IllegalStateException("not initialized yet !")
        }

        targetKilledCallback = killedCallback
        val candidate = backgroundProcessLru.firstOrNull {
            it.name != MatrixUtil.getProcessName(this)
                    && !ProcessSupervisor.config!!.lruKillerWhiteList.contains(it.name)
        }

        if (candidate != null) {
//            DispatchReceiver.dispatchKill(this, candidate.name, candidate.pid)
            ProcessSubordinate.manager.dispatchKill(recentScene, candidate.name, candidate.pid)
        } else {
            killedCallback.invoke(LRU_KILL_NOT_FOUND, null, -1)
        }
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
            }
            throw IllegalStateException("token with pid=$pid not found")
        }

        fun isEmpty(): Boolean =
            pidToToken.isEmpty() || pidToToken.all { it.key == Process.myPid() }

        fun removeToken(name: String): ProcessToken {
            val rm = nameToToken.remove(name)
            rm?.let {
                pidToToken.remove(rm.pid)
                return rm
            }
            throw IllegalStateException("token with name=$name not found")
        }
    }

    private class RemoteProcessLifecycleProxy(val token: ProcessToken) : StatefulOwner() {

        init {
            DispatcherStateOwner.addSourceOwner(
                token.statefulName,
                this
            )
        }

        companion object {

            private val processProxies by lazy { ConcurrentHashMap<ProcessToken, ConcurrentHashMap<String, RemoteProcessLifecycleProxy>>() }

            fun getProxy(token: ProcessToken) =
                processProxies.getOrPut(token, { ConcurrentHashMap() })
                    .getOrPut(token.statefulName, { RemoteProcessLifecycleProxy(token) })!!


            fun removeProxy(token: ProcessToken): Boolean {
                val proxies = processProxies.remove(token)
                if (proxies == null || proxies.isEmpty()) {
                    return false
                }
                proxies.forEach {
                    DispatcherStateOwner.removeSourceOwner(it.key, it.value)
                }
                return true
            }

            fun profile() {
                processProxies.forEach {
                    it.value.forEach { p ->
                        MatrixLog.d(TAG, "===> ${p.value}")
                    }
                }
            }
        }

        fun onStateChanged(state: Boolean) = if (state) {
            turnOn()
        } else {
            turnOff()
        }

        override fun toString(): String {
            return "OwnerProxy_${token.statefulName}_${active()}@${hashCode()}_${token.name}_${token.pid}"
        }
    }
}