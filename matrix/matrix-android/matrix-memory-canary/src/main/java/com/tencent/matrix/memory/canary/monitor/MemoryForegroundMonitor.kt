package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.Matrix
import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.CombinedProcessForegroundOwner

/**
 * Created by Yves on 2021/9/22
 */
object MemoryForegroundMonitor : IStateObserver {

    init {
        if (Matrix.isInstalled()) {
            CombinedProcessForegroundOwner.observeForever(this)
        }
    }

    interface IExtraInfoFactory {
        fun extra(): String
        fun extra1(): String
        fun extra2(): String
        fun extra3(): String
        fun extra4(): String
    }

    override fun on() {

    }

    override fun off() {

    }
}