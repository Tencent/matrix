package com.tencent.matrix.memory.canary.monitor

import com.tencent.matrix.lifecycle.IStateObserver
import com.tencent.matrix.lifecycle.owners.CombinedProcessForegroundOwner

/**
 * Created by Yves on 2021/9/22
 */
class MemoryBackgroundMonitor private constructor(): IStateObserver {

    companion object {
        private val sInstance = MemoryBackgroundMonitor()

        @JvmStatic
        fun get() = sInstance
    }

    init {
        CombinedProcessForegroundOwner.observeForever(this)
    }

    override fun on() {
        TODO("Not yet implemented")
    }

    override fun off() {
        TODO("Not yet implemented")
    }
}